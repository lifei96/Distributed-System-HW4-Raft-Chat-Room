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

// leader election, message passing, and failure recovery

// election timeout: The election timeout is the amount of time a follower waits until becoming a candidate.
// randomized to be between 150ms and 300ms. after timeout, vote itself and Request Vote messages to other nodes.
// and the node resets its election timeout.Once a candidate has a majority of votes it becomes leader.

// An entry is committed once a majority of followers acknowledge it

// A message have the following two fields.
const string MESSAGE_TYPE = "MESSAGE_TYPE";
const string MESSAGE_CONTENT = "MESSAGE_CONTENT";

// Values for MESSAGE_TYPE.
enum MessageType {
    int elect = 0;
    int vote;
    int msg;
};

enum NodeState {
    int follower = 0;
    int candidate;
    int leader;
};

class Entry: public{
    QString cmd;
    quint16 term;
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
    

public slots:
    void gotReturnPressed();

    void receiveDatagrams();

    void voteSelf();

    void voteSelf();

private:
    QTextEdit *textview;
    QLineEdit *textline;
    NetSocket *socket;
    QString originName;
    quint16 portNum;
    quint16 currentTerm;
    quint16 votedFor;
    Entry log[LOG_LIMITATION];
    quint16 commitIndex;
    quint16 lastApplied;
    quint16 nextIndex[5];
    quint16 matchIndex[5];
    QTimer *electionTimer;
    QTimer *heartbeatTimer;
    quint16 allNodes[5];
    NodeState state;
    static const int LOG_LIMITATION = 101;
};

#endif // P2PAPP_MAIN_HH
