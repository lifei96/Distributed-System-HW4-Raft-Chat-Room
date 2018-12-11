
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
        qDebug() << "node_id: " << portNum;
        setWindowTitle(portNum);
    }

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
            this, SLOT(becomeCandidate()));
    
}

Response ChatDialog::processCommand(QString cmd) {

    if (cmd.startsWith("START")) {
        getStart();

    } else if (cmd.startsWith("MSG")) {

    } else if (cmd.startsWith("GET_CHAT")) {
        for (int i = 1; i <= commitIndex; i++) {
            if (log[i].cmd.startsWith("MSG")) {
                qDebug() << log[i].node_id << " : " << log[i].cmd.mid(4);
            }
        }
    } else if (cmd.startsWith("STOP")) {
        state = NodeState.stop;
    } else if (cmd.startsWith("DROP")) {
        dropIndex[index] = true;
    } else if (cmd.startsWith("RESTORE")) {
        dropIndex[index] = false;
    } else if (cmd.startsWith("GET_NODES")) {

    }
}

void ChatDialog::gotReturnPressed() {
    // Initially, just echo the string locally.
    // Insert some networking code here...
    qDebug() << "New Command is: " << textline->text();

    // process the message vis socket
    QString cmd = textline->text();

    // Clear the textline to get ready for the next input message.
    textline->clear();

    // Redirect Request, after get new entry about this cmd, then execute
    redirectRequest(cmd);
}

void ChatDialog::getStart() {
    state = NodeState.follower;
    currentTerm = 0;
    votedFor = -1;
    commitIndex = 0;
    lastApplied = 0;
    currentLeader = -1;
    for (int i = myPortMin; i <= myPortMax; i++) {
        allNodes[i - myPortMin] = 1;
        /* Set true if drop from this node  */
        dropIndex[i - myPortMin] = false;
    }
}

void ChatDialog::sendHeartbeat() {
    if (state != NodeState.leader)
        return;
    currentTerm += 1;
    for (int p = myPortMin; p <= myPortMax; p++) {
        if (p != portNum) {
            sendRequest(MessageType.msg, p, QVariantMap());
        }
    }
    heartbeatTimer->start(HEARTBEATS);
}

void ChatDialog::becomeCandidate() {
    qDebug() << portNum << " become candidate";
    currentTerm += 1;
    currentLeader = -1;
    voteflag = 1 << (portNum - myPortMin);
    state = NodeState.candidate;
    allNodes[portNum - myPortMin] = 2;
    quint16 randomTimeout= qrand() % 150 + 150;
    electionTimer->start(randomTimeout);
    heartbeatTimer->start(HEARTBEATS);
    for (int i = myPortMin; i <= myPortMax; i++) {
        if (i != portNum && allNodes[i - myPortMin] != 0) {
            sendRequest(MessageType.elect, portNum, QVariantMap());
        }
    }
}

void ChatDialog::becomeLeader() {
    qDebug() << portNum << " become leader";
    currentLeader = -1;
    state = NodeState.leader;
    allNodes[portNum - myPortMin] = 3;
    quint16 randomTimeout= qrand() % 150 + 150;
    electionTimer->start(0);
    sendHeartbeat();
}

void ChatDialog::requestVote(quint16 term, quint16 candidateId, quint16 lastLogIndex, quint16 lastLogTerm) {
    if (term < currentTerm) {
        return Response(currentTerm, false);
    }
    if (term > currentTerm) {
        term = currentTerm;
        votedFor = candidateId;
        return Response(currentTerm, true);
    }
    if ((votedFor == -1 || votedFor == candidateId) && log[lastLogIndex] == lastLogTerm) {
        voteFor = candidateId;
        return Response(currentTerm, true);
    }
}

void ChatDialog::redirectRequest(QString cmd) {
    QVariantMap otherinfo;
    otherinfo["cmd"] = cmd;
    sendRequest(MessageType.request, currentLeader, otherinfo);
}

void ChatDialog::sendRequest(MessageType type, quint16 destPort, QVariantMap otherinfo) {
    QVariantMap message;
    QVariantMap parameters;
    parameters["term"] = currentTerm;
    if (destPort < socket->myPortMin || destPort > socket->myPortMax) {
        qDebug() << "inValid portNum: " << destPort;
        return;
    }
    switch(type) {
        case elect:
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
        case msg:
            parameters["leaderId"] = portNum;
            parameters["prevLogIndex"] = lastApplied;
            parameters["prevLogTerm"] = log[lastApplied].term;
            QVariantMap entries;
            entries["size"] = lastApplied - nextIndex[destPort - myPortMin] + 1;
            for(int i = nextIndex[destPort - myPortMin], index = 0; i <= lastApplied; i++) {
                QVariantMap entry;
                entry["term"] = log[i].term;
                entry["cmd"] = log[i].cmd;
                entry["node_id"] = log[i].node_id;
                entries[index] = append(entry);
            }
            parameters["entries"] = entries;
            parameters["leaderCommit"] = commitIndex;
            message["MSG"] = parameters;
            break;
        case ack:
            if (otherinfo.contains("cmd")) {
                parameters["cmd"] = otherinfo["cmd"];
            } else if (otherinfo.contains("status")) {
                parameters["status"] = otherinfo["status"];
            }
            parameters["voteFor"] = voteFor;
            parameters["sender"] = portNum;
            message["ACK"] = parameters;
            break;
    }
    serializeMessage(message, destPort);
}

Response ChatDialog::appendEntries(quint16 term, quint16 leaderId, quint16 prevLogIndex, quint16 prevLogTerm,
                                Entry *entries, quint16 leaderCommit) {
    if (term < currentTerm) {
        return Response(currentTerm, false);
    }
    if (log[prevLogIndex] != prevLogTerm) {
        return Response(currentTerm, false);
    }
    for (int i = 0; entries[i].term != term; i++) {
        log[prevLogIndex + i + 1] = entries[i];
    }
    log[prevLogIndex + i + 1] = entries[i];
    if (leaderCommit > commitIndex) {
        commitIndex = min(leaderCommit, prevLogIndex + i + 1);
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
            qDebug() << "receive datagram from: " << senderPort;
            deserializeMessage(datagram, senderPort);
        }
    }
}

void ChatDialog::serializeMessage(QVariantMap message, quint16 destPort) {

    QByteArray datagram;
    QDataStream outStream(&datagram, QIODevice::ReadWrite);
    outStream << message;

    qDebug() << "Sending message to port: " << destPort;

    socket->writeDatagram(
            datagram.data(),
            datagram.size(),
            QHostAddress::LocalHost,
            destPort);
}

void ChatDialog::deserializeMessage(QByteArray datagram, quint16 senderPort) {

    qDebug() << "deserialize Message";
    QVariantMap message;
    QDataStream inStream(&datagram, QIODevice::ReadOnly);
    inStream >> message;
    if (message.contains("ELECT")) {
        QVariantMap parameters = message["ELECT"];
        quint16 term = parameters["term"].toUint();
        quint16 candidateId = parameters["candidateId"].toUint();
        quint16 lastLogIndex = parameters["lastLogIndex"].toUint();
        quint16 lastLogTerm = parameters["lastLogTerm"].toUint();
        Response res = requestVote(term, candidateId, lastLogIndex, lastLogTerm);
        QVariantMap otherinfo;
        otherinfo["status"] = Response.status;
        sendRequest(MessageType.ack, candidateId, otherinfo);

    } else if (message.contains("REQUEST")) {
        if (state != leader) {
            qDebug() << "Redirect the command to the wrong port: " << portNum;
            return;
        }
        Entry newEntry;
        newEntry.term= parameters["term"].toUint();
        newEntry.node_id = parameters["node_id"].toUint();
        newEntry.cmd = parameters["cmd"].toString();
        
        //After get ACKs from all followers, then apply to local

    } else if (message.contains("MSG")) {
        QVariantMap parameters = message["MSG"];
        quint16 term = parameters["term"].toUint();
        quint16 leaderId = parameters["leaderId"].toUint();
        quint16 prevLogIndex = parameters["prevLogIndex"].toUint();
        quint16 prevLogTerm = parameters["prevLogTerm"].toUint();
        quint16 leaderCommit = parameters["leaderCommit"].toUint();
        Entry entries[LOG_LIMITATION];
        QVariantMap read_entries = parameters["entries"];
        quint16 size = read_entries["size"].toUint();
        for(int i = 0; i < size; i++) {
            QVariantMap read_entry = read_entries[i];
            Entry entry = Entry(read_entry["term"], read_entry["cmd"], read_entry["node_id"]);
            entries[index] = append(entry);
            // execute & apply to log
        }
        Response res = appendEntries(term, leaderId, prevLogIndex, prevLogTerm, entries, eleaderCommit);
        // send ack
        QVariantMap otherinfo;
        otherinfo["status"] = res.status;
        sendRequest(MessageType.ack, senderPort, otherinfo);
    } else if (message.contains("ACK")) {
        QVariantMap parameters = message["ACK"];
        quint16 term = parameters["term"].toUint();
        if (parameters.contains("cmd")) {
            // ACK for a redirected message
            QString cmd = parameters["cmd"].toString();

        } else (parameters.contains("status")) {
            bool status = parameters["status"].toBool();
            if (status) {
                voteflag |= 1 << (senderPort - myPortMin);
                int total = 0, vote = 0;
                for (int i = myPortMin; i <= myPortMax; i++) {
                    if (allNodes[i - myPortMin] != 0) {
                        total += 1;
                        if (voteflag & (1 << (i - myPortMin))) {
                            vote += 1;
                        }
                    }
                }
                if ((float)vote / (float)total > 0.5) {
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
