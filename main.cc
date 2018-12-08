
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
        originName =
                QHostInfo::localHostName() +
                "-" + QVariant(portNum).toString() +
                "-" + QUuid::createUuid().toString();
        qDebug() << "origin name: " << originName;
        setWindowTitle(originName);
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
            this, SLOT(voteSelf()));
    connect(heartbeatTimer, SIGNAL(timeout()),
            this, SLOT(()));
    
}

void ChatDialog::gotReturnPressed() {
    // Initially, just echo the string locally.
    // Insert some networking code here...
    qDebug() << "New Command is: " << textline->text();

    // process the message vis socket
    QString cmd = textline->text();

    if (cmd.contains("START")) {
        getStart();

    } else if (cmd.contains("MSG")) {

    } else if (cmd.contains("GET_CHAT")) {

    } else if (cmd.contains("STOP")) {

    } else if (cmd.contains("DROP")) {

    } else if (cmd.contains("RESTORE")) {

    } else if (cmd.contains("GET_NODES")) {

    }

    // Clear the textline to get ready for the next input message.
    textline->clear();
}

void ChatDialog::getStart() {
    state = NodeState.follower;
    quint16 randomTimeout= qrand() % 150 + 150;
    electionTimer->start(randomTimeout);
}
void ChatDialog::voteSelf() {
    self.state = NodeState.candidate;
    requestVote();
}

void ChatDialog::requestVote(quint16 term, quint16 candidateId, quint16 lastLogIndex, quint16 lastLogTerm) {
    if (term < currentTerm) {
        return currentTerm, false;
    }
    if ((votedFor == -1 || votedFor == candidateId) && log[lastLogIndex] == lastLogTerm) {
        return currentTerm, true;
    }
}

void ChatDialog::appendEntries(quint16 term, quint16 leaderId, quint16 prevLogIndex, quint16 prevLogTerm,
                                Entry *entries, quint16 leaderCommit) {
    if (term < currentTerm) {
        return currentTerm, false;
    }
    if (log[prevLogIndex] != prevLogTerm) {
        return currentTerm, false;
    }
    int i = 0;
    for (i = 0; entries[i].term != term; i++) {
        log[prevLogIndex + i + 1] = entries[i];
    }
    log[prevLogIndex + i + 1] = entries[i];
    if (leaderCommit > commitIndex) {
        commitIndex = min(leaderCommit, prevLogIndex + i + 1);
    }
    return term, success;

}
void ChatDialog::receiveDatagrams() {
    qDebug() << "receive datagram";
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
            deserializeMessage(datagram, senderHost, senderPort);
        }
    }
}

void ChatDialog::serializeMessage(
        QVariantMap message, QHostAddress destHost, quint16 destPort) {
    // To serialize a message you’ll need to construct a QVariantMap describing
    // the message
    qDebug() << "serialize Message";
    QByteArray datagram;
    QDataStream outStream(&datagram, QIODevice::ReadWrite);
    outStream << message;

    if (destPort < socket->myPortMin || destPort > socket->myPortMax) {
        destPort = findPort();
    }
    qDebug() << "Sending message to port: " << destPort;

    qint64 curTime = QDateTime::currentMSecsSinceEpoch();
    if (responseTimeDict.contains(destPort)) {
        responseTimeDict[destPort].setSendTime(curTime);
    } else {
        responseTimeDict.insert(destPort,
                                ResponseTime(destPort, curTime, curTime));
    }

    socket->writeDatagram(
            datagram.data(),
            datagram.size(),
            QHostAddress::LocalHost,
            destPort);
}

void ChatDialog::deserializeMessage(
        QByteArray datagram, QHostAddress senderHost, quint16 senderPort) {
    // using QDataStream, and handle the message as appropriate
    // containing a ChatText key with a value of type QString
    qint64 curTime = QDateTime::currentMSecsSinceEpoch();
    if (responseTimeDict.contains(senderPort)) {
        responseTimeDict[senderPort].setRecvTime(curTime);
    } else {
        responseTimeDict.insert(senderPort,
                                ResponseTime(senderPort, curTime, curTime));
    }
    qDebug() << "deserialize Message";
    QVariantMap message;
    QDataStream inStream(&datagram, QIODevice::ReadOnly);
    inStream >> message;
    if (message.contains("Want")) {
        receiveStatusMessage(message, senderHost, senderPort);
    } else {
        receiveRumorMessage(message, senderHost, senderPort);
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
