#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>
#include <QTimer>
#include <QHostInfo>
#include <QUuid>
#include <QDateTime>
#include <QtAlgorithms>

// A message have the following two fields.
// const string MESSAGE_TYPE = "MESSAGE_TYPE";
// const string MESSAGE_CONTENT = "MESSAGE_CONTENT";

// Values for MESSAGE_TYPE.
enum MessageType {
    elect = 0,
    request,
    msg,
    ack,
};

enum NodeState {
    follower = 0,
    candidate,
    leader,
    stop,
};

class Entry {
public:
    Entry(){};
    Entry(quint16 _term, QString _cmd, quint16 _node_id): term(_term), cmd(_cmd), node_id(_node_id){}
    quint16 term;
    QString cmd;
    quint16 node_id;
};

class Response {
public:
    Response(){};
    Response(quint16 _term, bool _status): term(_term), status(_status){}
    quint16 term;
    bool status;
};

class NetSocket : public QUdpSocket {
    Q_OBJECT

public:
    NetSocket();

    // Bind this socket to a P2Papp-specific default port.
    bool bind();

public:
    quint16 myPortMin, myPortMax;
    quint16 port;
};

class ChatDialog : public QDialog {
    Q_OBJECT

public:
    ChatDialog();

private:
    void startElectionTimer();
    Response processCommand(QString cmd, quint16 node_id);
    void getStart();
    void becomeLeader();
    Response requestVote(quint16 term, quint16 candidateId, quint16 lastLogIndex, quint16 lastLogTerm);
    void redirectRequest(QString cmd);
    void sendRequest(MessageType type, quint16 destPort, QVariantMap otherinfo);
    Response appendEntries(quint16 term, quint16 leaderId, quint16 prevLogIndex, quint16 prevLogTerm,
                                Entry *entries, quint16 leaderCommit);
    void serializeMessage(QVariantMap message, quint16 destPort);
    void deserializeMessage(QByteArray datagram, quint16 senderPort);


public slots:
    void gotReturnPressed();

    void receiveDatagrams();

    void becomeCandidate();

    void sendHeartbeat();

private:
    static const int LOG_LIMITATION = 101;
    static const int HEARTBEATS = 1500;
    static const int ELECTION_TIME = 3000;
    QTextEdit *textview;
    QLineEdit *textline;
    NetSocket *socket;
    quint16 myPortMin, myPortMax;
    quint16 portNum;
    quint16 currentTerm;
    quint16 currentLeader;
    qint16 voteflag;
    qint16 votedFor;
    Entry log[LOG_LIMITATION];
    QVector<Entry>cachedLog;
    quint16 commitIndex;
    quint16 lastApplied;
    /*  record state for all nodes: 0 close 1 follower 2 candidate 3 leader     */
    quint16 allNodes[5];
    quint16 nextIndex[5];
    quint16 matchIndex[5];
    bool dropIndex[5];
    QTimer *electionTimer;
    QTimer *heartbeatTimer;
    NodeState state;
};

#endif // P2PAPP_MAIN_HH
