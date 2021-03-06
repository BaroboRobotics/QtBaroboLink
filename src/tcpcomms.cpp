#include "qtrobotmanager.h"
#include "tcpcomms.h"

CommsRobotClient::CommsRobotClient(QObject *parent) : QObject(parent)
{
}

CommsRobotClient::~CommsRobotClient()
{
}

void CommsRobotClient::init(QTcpSocket *socket, RecordMobot* robot)
{
  sock_ = socket;
  robot_ = robot;
  QObject::connect(socket, SIGNAL(readyRead()), this, SLOT(bytesFromClientReady()));
  QObject::connect(socket, SIGNAL(disconnected()), this, SLOT(disconnect()));
}

void CommsRobotClient::bytesFromClientReady()
{
  int rc;
  static QByteArray buf;
  QByteArray tmpbuf;
  quint8 bytebuf[128];
  tmpbuf = sock_->readAll();
  buf += tmpbuf;
  if( (buf.size() > 2) &&
      (buf.size() >= buf.at(1)) )
  {
    /* Received whole message */
    memcpy(bytebuf, buf.data(), buf.size());
    rc = robot_->transactMessage(bytebuf[0], &bytebuf[2], bytebuf[1]-3);
    if(rc) return;
    rc = sock_->write((const char*)&bytebuf[2], bytebuf[3]);
    buf = buf.right(buf.size() - buf.at(1));
  }
}

void CommsRobotClient::disconnect()
{
  robot_->setBound(false);
}

CommsForwarding::CommsForwarding(QObject *parent) : QObject(parent)
{
  server_ = new QTcpServer();
}

CommsForwarding::~CommsForwarding()
{
}

void CommsForwarding::start(quint16 port)
{
  if(server_->isListening()) return;
  server_->listen(QHostAddress::Any, port);
  QObject::connect(server_, SIGNAL(newConnection()), this, SLOT(newConnection()));
}

void CommsForwarding::stop()
{
  server_->close();
}

void CommsForwarding::newConnection()
{
  qDebug() << "Received new connection.";
  /* Listener received a new connection. */
  /* See if we can get an unbound robot */
  QTcpSocket* sock;
  sock = server_->nextPendingConnection();
  RecordMobot* robot = robotManager()->getUnboundMobot();
  if(NULL == robot) {
    /* Immediately close the connection and return */
    sock->close();
    return;
  }
  /* Create new CommsRobotClient object */
  robot->setBound(true);
  CommsRobotClient* client = new CommsRobotClient();
  client->init(sock, robot);
  clients_.prepend(client);
  qDebug() << "Finished receiving new connection.";
}

