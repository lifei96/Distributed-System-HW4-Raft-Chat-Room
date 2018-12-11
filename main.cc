
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

void ChatDialog::gotReturnPressed() {
    // Initially, just echo the string locally.
    // Insert some networking code here...
    qDebug() << "New Command is: " << textline->text();

    // process the message vis socket
    QString cmd = textline->text();

    if (cmd.startsWith("START")) {
        getStart();

    } else if (cmd.startsWith("MSG")) {

    } else if (cmd.startsWith("GET_CHAT")) {

    } else if (cmd.startsWith("STOP")) {

    } else if (cmd.startsWith("DROP")) {

    } else if (cmd.startsWith("RESTORE")) {

    } else if (cmd.startsWith("GET_NODES")) {

    }

    // Clear the textline to get ready for the next input message.
    textline->clear();
}

void ChatDialog::getStart() {
    state = NodeState.follower;
    currentTerm = 0;
    votedFor = -1;
    commitIndex = 0;
    lastApplied = 0;
}
void ChatDialog::becomeCandidate() {
    currentTerm += 1;
    // for (int i = myPortMin; i <= myPortMax; i++) {
    //     vote_for_me(i);
    // }
    currentLeader = -1;
    state = NodeState.candidate;
    quint16 randomTimeout= qrand() % 150 + 150;
    electionTimer->start(randomTimeout);
    for (int i = myPortMin; i <= myPortMax; i++) {
        if (i != portNum && node_is_active(i) && node_is_voting(i)) {
            sendRequest(MessageType.elect, portNum, currentTerm, portNum);
        }
    }
}

void ChatDialog::startElection() {

}

void ChatDialog::requestVote(quint16 term, quint16 candidateId, quint16 lastLogIndex, quint16 lastLogTerm) {
    if (term < currentTerm) {
        return Response(currentTerm, false);
    }
    if ((votedFor == -1 || votedFor == candidateId) && log[lastLogIndex] == lastLogTerm) {
        return Response(currentTerm, true);
    }
}

void get_chat()

Response ChatDialog::sendRequest(MessageType type, quint16 destPort) {
    QVariantMap message;
    QVariantMap parameters;
    switch(type) {
        case elect:
            parameters["term"] = currentTerm;
            parameters["candidateId"] = portNum;
            parameters["lastLogIndex"] = lastApplied;
            parameters["lastLogTerm"] = log[lastApplied].term;
            message["ELECT"] = parameters;
            break;
        case vote:
            parameters["term"] = currentTerm;
            parameters["candidateId"] = portNum;
            parameters["lastLogIndex"] = lastApplied;
            parameters["lastLogTerm"] = log[lastApplied].term;
            message["VOTE"] = parameters;
            break;
        case msg:
            parameters["term"] = currentTerm;
            parameters["leaderId"] = portNum;
            parameters["prevLogIndex"] = lastApplied;
            parameters["prevLogTerm"] = log[lastApplied].term;
            parameters["entries"] = ???;
            parameters["leaderCommit"] = commitIndex;
            message["MSG"] = parameters;
            break;
        case ack:
            parameters["term"] = currentTerm;
            parameters["status"] = success or fail;
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
    int i = 0;
    for (i = 0; entries[i].term != term; i++) {
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

    if (destPort < socket->myPortMin || destPort > socket->myPortMax) {
        qDebug() << "inValid portNum: " << destPort;
    }
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
        
    } else if (message.contains("VOTE")) {
    } else if (message.contains("MSG")) {
        
    } else if (message.contains("ACK")) {

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
