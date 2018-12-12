
#include <unistd.h>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>

#include "main.hh"

ChatDialog::ChatDialog() {
    setWindowTitle("P2Papp");

    socket = new NetSocket();
    if (!socket->bind()) {
        exit(1);
    } else {
        portNum = socket->port;
        /* use port as a node id and user name  */
        qDebug() << "node_id:" << portNum;
        setWindowTitle(QString::number(portNum));
    }
    myPortMin = socket->myPortMin;
    myPortMax = socket->myPortMax;

    electionTimer = new QTimer(this);
    heartbeatTimer = new QTimer(this);
    // Read-only text box where we display messages from everyone.
    // This widget expands both horizontally and vertically.
    textview = new QTextEdit(this);
    textview->setReadOnly(true);

    // Small text-entry box the user can enter messages.
    // This widget normally expands only horizontally,
    // leaving extra vertical space for the textview widget.
    //
    // You might change this into a read/write QTextEdit,
    // so that the user can easily enter multi-line messages.
    textline = new QLineEdit(this);

    // Lay out the widgets to appear in the main window.
    // For Qt widget and layout concepts see:
    // http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(textview);
    layout->addWidget(textline);
    setLayout(layout);

    // Register a callback on the textline's returnPressed signal
    // so that we can send the message entered by the user.
    connect(textline, SIGNAL(returnPressed()),
            this, SLOT(gotReturnPressed()));
    connect(socket, SIGNAL(readyRead()),
            this, SLOT(receiveDatagrams()));
    connect(electionTimer, SIGNAL(timeout()),
            this, SLOT(becomeCandidate()));
    connect(heartbeatTimer, SIGNAL(timeout()),
            this, SLOT(sendHeartbeat()));

    getStart();
}

void ChatDialog::processCommand(Entry entry, bool redirect) {
    QString cmd = entry.cmd;
    qDebug() << "Process command:" << cmd;
    // Execute cmd for other nodes
    if (cmd.startsWith("START")) {
        getStart();
    } else if (cmd.startsWith("STOP")) {
        state = stop;
        currentLeader = -1;
        allNodes[portNum - myPortMin] = 0;
        textview->append("STOP");
        electionTimer->stop();
    } else if (cmd.startsWith("GET_CHAT")) {
        for (int i = 1; i <= commitIndex; i++) {
            if (log[i].cmd.startsWith("MSG")) {
                qDebug() << log[i].node_id << ":" << log[i].cmd.mid(4);
            }
        }
    } else if (cmd.startsWith("DROP")) {
        int index = cmd.mid(5).toInt();
        if (index >= myPortMin && index <= myPortMax) {
            dropIndex[index - myPortMin] = true;
        } else {
            qDebug() << "invalid node_id";
        }
    } else if (cmd.startsWith("RESTORE")) {
        int index = cmd.mid(5).toInt();
        if (index >= myPortMin && index <= myPortMax) {
            dropIndex[index - myPortMin] = false;
        } else {
            qDebug() << "invalid node_id";
        }
    } else if (cmd.startsWith("GET_NODES")) {
        for (int p = myPortMin; p <= myPortMax; p++) {
            switch(allNodes[p - myPortMin]) {
                case 0:
                    qDebug() << p << ": stop";
                    break;
                case 1:
                    qDebug() << p << ": follower";
                    break;
                case 2:
                    qDebug() << p << ": candidate";
                    break;
                case 3:
                    qDebug() << p << ": leader";
                    break;
            }
        }
    } else if (cmd.startsWith("MSG")) {
        if (redirect) {
            qDebug() << "Redirect command:" << cmd;
            cachedLog.append(Entry(currentTerm, cmd, portNum));
            redirectRequest();
        } else {
            qDebug() << "Process command for leader:" << cmd;
            if (entry.node_id == portNum) {
                textview->append(QString::number(entry.node_id) + "(me): " + cmd.mid(4));
            } else {
                textview->append(QString::number(entry.node_id) + " : " + cmd.mid(4));
            }

            // if (state != leader){
            //     // Leader also redirect to self
            //     qDebug() << "Redirect command:" << cmd;
            //     cachedLog.append(Entry(currentTerm, cmd, portNum));
            //     redirectRequest();
            // } else {
            //     qDebug() << "Process command in leader";
            //     log[++receiveIndex] = Entry(currentTerm, cmd, portNum);
            //     nextIndex[portNum - myPortMin] = receiveIndex + 1;
            //     matchIndex[portNum - myPortMin] = receiveIndex;
            //     for (int p = myPortMin; p <= myPortMax; p++) {
            //         if (p != portNum) {
            //             sendRequest(msg, p, QVariantMap());
            //         }
            //     }
            // }
        }
    } else {
        qDebug() << "unrecoganized message" << cmd;
    }

}

void ChatDialog::gotReturnPressed() {
    // Initially, just echo the string locally.
    // Insert some networking code here...
    qDebug() << "New Command is:" << textline->text();

    // process the message vis socket
    QString cmd = textline->text();

    // Clear the textline to get ready for the next input message.
    textline->clear();

    // Redirect Request, after get new entry about this cmd, then execute
    processCommand(Entry(currentTerm, cmd, portNum), true);

}

void ChatDialog::startElectionTimer() {
    quint16 randomTimeout= qrand() % ELECTION_TIME + ELECTION_TIME;
    electionTimer->start(randomTimeout);
}

void ChatDialog::getStart() {
    state = follower;
    currentTerm = 0;
    votedFor = -1;
    commitIndex = 0;
    lastApplied = 0;
    currentLeader = -1;
    receiveIndex = 0;
    textview->clear();
    for (int p = myPortMin; p <= myPortMax; p++) {
        allNodes[p - myPortMin] = 1;
        /* Set true if drop from this node  */
        dropIndex[p - myPortMin] = false;
        nextIndex[p - myPortMin] = 1;
        matchIndex[p - myPortMin] = 0;
    }
    startElectionTimer();
}

void ChatDialog::sendHeartbeat() {
    if (state != leader)
        return;
    qDebug() << "Heartbeat";
    for (int p = myPortMin; p <= myPortMax; p++) {
        if (p != portNum) {
            sendRequest(msg, p, QVariantMap());
        }
    }
    heartbeatTimer->start(HEARTBEATS);
}

void ChatDialog::becomeFollower() {
    qDebug() << portNum << " become follower";
    state = follower;
    allNodes[portNum - myPortMin] = 1;
}

void ChatDialog::becomeCandidate() {
    qDebug() << portNum << " become candidate";
    currentTerm += 1;
    currentLeader = -1;
    voteflag = 1 << (portNum - myPortMin);
    state = candidate;
    for (int p = 0; p < 5; p++) {
        allNodes[p] = 1;
    }
    allNodes[portNum - myPortMin] = 2;
    startElectionTimer();
    for (int p = myPortMin; p <= myPortMax; p++) {
        if (p != portNum) {
            sendRequest(elect, p, QVariantMap());
        }
    }
}

void ChatDialog::becomeLeader() {
    qDebug() << portNum << " become leader";
    currentLeader = portNum;
    state = leader;
    allNodes[portNum - myPortMin] = 3;
    electionTimer->stop();
    sendHeartbeat();
}

Response ChatDialog::requestVote(quint16 term, quint16 candidateId, quint16 lastLogIndex, quint16 lastLogTerm) {
    if (term < currentTerm) {
        return Response(currentTerm, false);
    }
    if (term > currentTerm) {
        term = currentTerm;
        votedFor = candidateId;
        currentLeader = -1;
        for (int p = 0; p < 5; p++) {
            allNodes[p] = 1;
        }
        allNodes[candidateId - myPortMin] = 2;
        return Response(currentTerm, true);
    }
    if ((votedFor == -1 || votedFor == candidateId) && log[lastLogIndex].term == lastLogTerm) {
        votedFor = candidateId;
        for (int p = 0; p < 5; p++) {
            allNodes[p] = 1;
        }
        allNodes[candidateId - myPortMin] = 2;
        return Response(currentTerm, true);
    }
    return Response(currentTerm, false);
}

void ChatDialog::redirectRequest() {
    for (int i = 0; i < cachedLog.size(); i++) {
        QVariantMap otherinfo;
        otherinfo["cmd"] = cachedLog[i].cmd;
        // TODO: what if the pending request has a different term?
        if (currentLeader != -1) {
            sendRequest(request, (quint16)currentLeader, otherinfo);
        }
    }
}

void ChatDialog::sendRequest(MessageType type, quint16 destPort, QVariantMap otherinfo) {
    
    if (destPort < socket->myPortMin || destPort > socket->myPortMax) {
        qDebug() << "Invalid portNum:" << destPort;
        return;
    }
    if (state == stop) {
        qDebug() << "Stop send message";
        return;
    }
    QVariantMap message;
    QVariantMap parameters = otherinfo;
    parameters["term"] = currentTerm;
    switch(type) {
        case elect:
            if (state != candidate) {
                qDebug() << "Invalid send elect to:" << destPort;
                break;
            }
            parameters["candidateId"] = portNum;
            parameters["lastLogIndex"] = lastApplied;
            parameters["lastLogTerm"] = log[lastApplied].term;
            message["ELECT"] = parameters;
            break;
        case request:
            parameters["message"] = otherinfo["message"];
            parameters["node_id"] = portNum;
            message["REQUEST"] = parameters;
            break;
        case msg: {
            if (state != leader) {
                qDebug() << "Invalid send msg to:" << destPort;
                break;
            }
            parameters["leaderId"] = portNum;
            parameters["prevLogIndex"] = nextIndex[destPort - myPortMin] - 1;
            parameters["prevLogTerm"] = log[nextIndex[destPort - myPortMin] - 1].term;
            QVariantMap entries;
            entries["size"] = int(receiveIndex - nextIndex[destPort - myPortMin] + 1);
            for(int i = nextIndex[destPort - myPortMin]; i <= receiveIndex; i++) {
                QVariantMap entry;
                entry["term"] = log[i].term;
                entry["cmd"] = log[i].cmd;
                entry["node_id"] = log[i].node_id;
                qDebug() << "Send Entry:" << log[i].cmd;
                entries[QString::number(i)] = entry;
            }
            parameters["entries"] = entries;
            parameters["leaderCommit"] = commitIndex;
            message["MSG"] = parameters;
            break;
            }
        case ack:
            // for (QVariantMap::iterator iter = otherinfo.begin(); iter != otherinfo.end(); iter++) {
            //     parameters[iter->first] = iter->second;
            // }
            message["ACK"] = parameters;
            break;
        default:
            break;
    }
    serializeMessage(message, destPort);
}

Response ChatDialog::appendEntries(quint16 term, quint16 leaderId, quint16 prevLogIndex, quint16 prevLogTerm,
                                Entry *entries, quint16 leaderCommit, quint16 size) {
    if (term < currentTerm) {
        qDebug() << "Error: appendEntries fails because term outdate!" << currentTerm;
        return Response(currentTerm, false);
    }
    if (log[prevLogIndex].term != prevLogTerm && prevLogIndex != 0) {
        qDebug() << "Error: appendEntries fails because term not match!" << prevLogIndex;
        return Response(currentTerm, false);
    }
    /* prevLogIndex is nextIndex - 1 in leader(should be lastApplied)   */
    for (int i = prevLogIndex + 1; i <= prevLogIndex + size; i++) {
        log[i] = entries[i];
    }
    receiveIndex = prevLogIndex + size;
    qDebug() << "TEST: lastApplied =" << lastApplied;
    for (int i = lastApplied + 1; i <= leaderCommit; i++) {
        qDebug() << "Execute log" << i;
        processCommand(log[i], false);
    }
    qDebug() << "leaderCommit =" << leaderCommit << ", receiveIndex =" << receiveIndex;
    lastApplied = leaderCommit;
    if (leaderCommit > commitIndex) {
        commitIndex = leaderCommit;
        // TODO: why can be the last new entry, it is not committed by the majority
        // qMin(leaderCommit, (quint16)(prevLogIndex + i + 1));
    }
    return Response(term, true);

}

// Datagram send/receive functions

void ChatDialog::receiveDatagrams() {
    while (socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(socket->pendingDatagramSize());
        QHostAddress senderHost;
        quint16 senderPort;
        if (socket->readDatagram(
                datagram.data(),
                datagram.size(),
                &senderHost,
                &senderPort) != -1) {
            deserializeMessage(datagram, senderPort);
        }
    }
}

void ChatDialog::serializeMessage(QVariantMap message, quint16 destPort) {

    QByteArray datagram;
    QDataStream outStream(&datagram, QIODevice::ReadWrite);
    outStream << message;

    qDebug() << "Sending message to port:" << destPort;

    socket->writeDatagram(
            datagram.data(),
            datagram.size(),
            QHostAddress::LocalHost,
            destPort);
}

void ChatDialog::deserializeMessage(QByteArray datagram, quint16 senderPort) {

    QVariantMap message;
    QDataStream inStream(&datagram, QIODevice::ReadOnly);
    inStream >> message;
    quint16 term;
    if (dropIndex[senderPort - myPortMin])
        return;
    if (state == stop) {
        qDebug() << "Stop accept message";
        return;
    }
    startElectionTimer();
    if (message.contains("ELECT")) {
        // This is follower receive requestVote RPC from candidate
        qDebug() << "Receiving elect from port:" << senderPort;
        QVariantMap parameters = qvariant_cast<QVariantMap>(message["ELECT"]);
        term = parameters["term"].toUInt();
        quint16 candidateId = parameters["candidateId"].toUInt();
        quint16 lastLogIndex = parameters["lastLogIndex"].toUInt();
        quint16 lastLogTerm = parameters["lastLogTerm"].toUInt();
        Response res = requestVote(term, candidateId, lastLogIndex, lastLogTerm);
        QVariantMap otherinfo;
        otherinfo["status"] = res.status;
        otherinfo["votedFor"] = votedFor;
        sendRequest(ack, senderPort, otherinfo);

    } else if (message.contains("REQUEST")) {
        // This is leader receive redirect request from clients
        qDebug() << "Receiving request from port:" << senderPort;
        if (state != leader) {
            qDebug() << "Redirect the command to the wrong port:" << portNum;
            return;
        }
        QVariantMap parameters = qvariant_cast<QVariantMap>(message["REQUEST"]);
        Entry newEntry;
        newEntry.term = parameters["term"].toUInt();
        newEntry.node_id = parameters["node_id"].toUInt();
        newEntry.cmd = parameters["cmd"].toString();
        
        term = parameters["term"].toUInt();

        log[++receiveIndex] = newEntry;

        QVariantMap otherinfo;
        otherinfo["cmd"] = newEntry.cmd;
        sendRequest(ack, senderPort, otherinfo);

        for (int p = myPortMin; p <= myPortMax; p++) {
            if (p != portNum) {
                sendRequest(msg, p, QVariantMap());
            }
        }

    } else if (message.contains("MSG")) {
        // This is follower receive appendEntries RPC from leader
        qDebug() << "Receiving msg from port:" << senderPort;
        if (currentLeader == -1) {
            currentLeader = senderPort;
            for(int i = 0; i < 5; i++)
                allNodes[i] = 1;
            allNodes[senderPort - myPortMin] = 3;
        }
        QVariantMap parameters = qvariant_cast<QVariantMap>(message["MSG"]);
        term = parameters["term"].toUInt();
        quint16 leaderId = parameters["leaderId"].toUInt();
        quint16 prevLogIndex = parameters["prevLogIndex"].toUInt();
        quint16 prevLogTerm = parameters["prevLogTerm"].toUInt();
        quint16 leaderCommit = parameters["leaderCommit"].toUInt();
        Entry entries[LOG_LIMITATION];
        QVariantMap read_entries;
        quint16 size = 0;
        if (parameters.contains("entries")) {
            read_entries = qvariant_cast<QVariantMap>(parameters["entries"]);
            size = read_entries["size"].toInt();
            qDebug() << "Receive Entry number:" << size;
            for(int i = prevLogIndex + 1; i <= prevLogIndex + size; i++) {
                QVariantMap read_entry = qvariant_cast<QVariantMap>(read_entries[QString::number(i)]);
                Entry entry = Entry(read_entry["term"].toUInt(), read_entry["cmd"].toString(), read_entry["node_id"].toUInt());
                entries[i] = entry;
                qDebug() << "Receive Entry:" << entries[i].cmd;
            }
        } else {
            qDebug() << "Receiving Heartbeat";
        }
        Response res = appendEntries(term, leaderId, prevLogIndex, prevLogTerm, entries, leaderCommit, size);
        qDebug() << "AppendEntries is success? " << (int)res.status;
        QVariantMap otherinfo;
        otherinfo["status"] = res.status;
        otherinfo["received"] = receiveIndex;
        sendRequest(ack, senderPort, otherinfo);
        // Everytime receive msg, try to send pending request to leader
        redirectRequest();
    } else if (message.contains("ACK")) {
        /* This are three ACKs:
        /. 1. Leader get ACK from follower with their receiveIndex
        /. 2. Follower get ACK from leader about their redirect request
        /. 3. Candidate get ACK from follower about the vote result
        */
        qDebug() << "Receiving ack from port:" << senderPort;
        QVariantMap parameters = qvariant_cast<QVariantMap>(message["ACK"]);
        term = parameters["term"].toUInt();
        if (parameters.contains("received")) {
            if (state != leader) {
                qDebug() << "Error: receive ACK for reveiceIndex!";
            }
            quint16 received = parameters["received"].toUInt();
            bool status = parameters["status"].toBool();
            qDebug() << "receiveIndex =" << received << " from " << senderPort;
            if (status) {
                nextIndex[senderPort - myPortMin] = received + 1;
            } else {
                // If AppendEntries fails because of log inconsistency: decrement nextIndex and retry
                nextIndex[senderPort - myPortMin] --;
                sendRequest(msg, senderPort, QVariantMap());
            }
            matchIndex[senderPort - myPortMin] = qMax(matchIndex[senderPort - myPortMin], received);
            for (int p = myPortMin; p <= myPortMax; p++) {
                int votes = 0;
                for (int q = myPortMin; q <= myPortMax; q++) {
                    if (matchIndex[q - myPortMin] >= matchIndex[p - myPortMin]) {
                        votes ++;
                    }
                }
                if (votes >= 3 && matchIndex[p - myPortMin] > commitIndex) {
                    commitIndex = matchIndex[p - myPortMin];
                    qDebug() << "myleaderCommit = " << commitIndex;
                    for (int i = lastApplied + 1; i <= commitIndex; i++) {
                        processCommand(log[i], false);
                    }
                    lastApplied = commitIndex;
                }
            }
        } else if (parameters.contains("cmd")) {
            // ACK for a redirected message, should check in order
            QString cmd = parameters["cmd"].toString();
            qDebug() << "redirect success for" << cmd;
            for (int i = 0; i < cachedLog.size(); i++) {
                if (cmd == cachedLog[i].cmd)
                    cachedLog.remove(i);
            }
        } else if (parameters.contains("votedFor")) {
            if (state != candidate && state != leader) {
                qDebug() << "Error: receive ACK for candidate!";
            }
            quint16 vote_who = parameters["votedFor"].toUInt();
            qDebug() << "vote:" << vote_who;
            if (vote_who == portNum) {
                voteflag |= 1 << (senderPort - myPortMin);
                int votes = 0;
                for (int p = myPortMin; p <= myPortMax; p++) {
                    if (voteflag & (1 << (p - myPortMin))) {
                            votes += 1;
                    }
                }
                qDebug() << "vote for me:" << votes;
                if (votes >= 3 && state != leader) {
                    becomeLeader();
                }
            }
        }

        // both ACK from followers or results from leader
    }
    // if (commitIndex > lastApplied) {
    //     for (int i = lastApplied; i <= commitIndex; i++) {
    //         log[i] = entries[?];
    //     }
    // }
    if (term > currentTerm) {
        currentTerm = term;
        state = follower;
    }
}

NetSocket::NetSocket() {
    // Pick a range of four UDP ports to try to allocate by default,
    // computed based on my Unix user ID.
    // This makes it trivial for up to four P2Papp instances per user
    // to find each other on the same host,
    // barring UDP port conflicts with other applications
    // (which are quite possible).
    // We use the range from 32768 to 49151 for this purpose.
    myPortMin = 32768 + (getuid() % 4096) * 4;
    myPortMax = myPortMin + 4;
}

bool NetSocket::bind() {
    // Try to bind to each of the range myPortMin..myPortMax in turn.
    for (int p = myPortMin; p <= myPortMax; p++) {
        if (QUdpSocket::bind(p)) {
            qDebug() << "bound to UDP port " << p;
            port = p;
            return true;
        }
    }

    qDebug() << "Oops, no ports in my default range " << myPortMin
             << "-" << myPortMax << " available";
    return false;
}

int main(int argc, char **argv) {
    // Initialize Qt toolkit
    QApplication app(argc, argv);

    // Create an initial chat dialog window
    ChatDialog dialog;
    dialog.show();

    // Enter the Qt main loop; everything else is event driven
    return app.exec();
}
