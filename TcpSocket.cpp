#include <iostream>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "Exception.h"
#include "TcpSocket.h"

using namespace std;

TcpSocket::TcpSocket()
    : sockaddr(*new sockaddr_in),
      socklen(sizeof(socklen_t)),
      sockfd(socket (AF_INET, SOCK_STREAM, 0))
{
    if (sockfd == -1)
        throw *new SocketException("Could not create socket", errno);

    bzero(&sockaddr, sizeof(struct sockaddr_in));

    int on;
    if (setsockopt (sockfd,
            SOL_SOCKET,
            SO_REUSEADDR,
            (const char*) &on,
            sizeof (on)) != 0)
        throw *new SocketException("Could not setsockopt", errno);
}

TcpSocket::TcpSocket(int fd, sockaddr_in& addr, socklen_t& len)
    : sockaddr(sockaddr), socklen(len),sockfd(fd) {}

TcpSocket::~TcpSocket(){
    if (::close(sockfd) != 0)
        throw *new SocketException("Could not close socket", errno);
}

void TcpSocket::connect(const string host, const int port){
    if (bound()) throw *new SocketException("Socket already bound");

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port   = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &sockaddr.sin_addr) == -1)
        throw *new SocketException("Could not convert address",errno);

    if (::connect(sockfd, reinterpret_cast<struct sockaddr *>(&sockaddr), sizeof(sockaddr)) != 0)
        throw *new SocketException("Could not connect()", errno);

    setStatus(CONNECTED);
}

void TcpSocket::bind(const int port){
    if (bound()) throw *new SocketException("Socket already bound");

    sockaddr.sin_family         = AF_INET;
    sockaddr.sin_addr.s_addr    = INADDR_ANY;
    sockaddr.sin_port           = htons(port);

    if (::bind(sockfd, reinterpret_cast<struct sockaddr *>(&sockaddr), sizeof(sockaddr)) != 0)
        throw *new SocketException("Could not bind()", errno);

    setStatus(BOUND);
}

void TcpSocket::listen() const {
    if (!bound()) throw *new SocketException("Socket isn't bound yet");

    // TODO: review if MAXCONNECTIONS should be a parameter to the socket.
    if (::listen (sockfd, MAXCONNECTIONS) != 0)
        throw *new SocketException("Cound not accept()", errno);
}

TcpSocket& TcpSocket::accept() const {
    if (!listening()) throw *new SocketException("Socket isn't listening");

    int clifd;
    sockaddr_in& cliaddr  = *(new sockaddr_in);
    socklen_t& cliaddrlen = *(new socklen_t(sizeof(sockaddr_in)));

    bzero(&cliaddr, sizeof(sockaddr_in));

    if ((clifd = ::accept (sockfd,
                reinterpret_cast<struct sockaddr *>(&cliaddr),
                &cliaddrlen)) == -1)
        throw *new SocketException("Could not accept()");

    TcpSocket& clisocket = *new TcpSocket(clifd, cliaddr, cliaddrlen);
    clisocket.setStatus(CONNECTED);
    return clisocket;
}

void TcpSocket::send(const string s) const{
    cout << "Would be sending: " << s << " to socket\n";
}

void TcpSocket::recv(string& s) const{
    cout << "Would be sending: " << s << " to socket\n";
}

bool TcpSocket::bound() const {
    return status==BOUND;
}

bool TcpSocket::listening() const {
    return status==LISTENING;
}

bool TcpSocket::connected() const {
    return status==CONNECTED;
}

void TcpSocket::setStatus(const enum status_t newStatus) {
    status=newStatus;
}

const string& TcpSocket::printStatus() const{
    switch (status){
    case BOUND:
        return "Bound";
    case CONNECTED:
        return "Connected";
    case LISTENING:
        return "Listening";
    default:
        return "Unknown";
    }
}

std::ostream& TcpSocket::out(std::ostream& os) const{
    string s;

    s << "[TcpSocket: status:" << printStatus();
    if (status!=UNKNOWN){

        s << " address: ";
        char buff[MAXLINE];
        if (inet_ntop(AF_INET, sockaddr, buff, MAXLINE)!=NULL)
            s << buff;
        else
            s << "<internal error>";
    } else { s << " unknown status"; }
    s << "]";

    return os << s;
}

std::ostream& operator<<(std:stream& os, const TcpSocket& sock){
    return sock.out(os);
}

// Unit tests

bool simpleAcceptTest(){
    try {
        TcpSocket serverSocket;

        serverSocket.bind(34343);
        serverSocket.listen();

        cout << "  Bound to localhost:34343"
             << "  Waiting for accept() to return\n. ";
        TcpSocket& connectedSocket = serverSocket.accept();
        cout << "  Connection accepted: " << connectedSocket << endl << "Closing!\n";
        return true;
    } catch (SocketException& e){
        cout << "  " << e << endl;
        return false;
    }
}

int main(int argc, char* argv[]){
    cout << "Starting TcpSocket unit tests\n";
    simpleAcceptTest();
}












