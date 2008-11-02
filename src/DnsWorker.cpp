// stdl includes
#include <sstream>

// Project includes
#include "DnsWorker.h"

using namespace std;

DnsWorker::DnsWorker(DnsResolver& _resolver, const size_t _maxmessage)
    : resolver(_resolver), maxmessage(_maxmessage){
    stop_flag = false;
    retval = -1;
    id = uniqueid++;
}

DnsWorker::~DnsWorker(){}

void DnsWorker::rest(){stop_flag = true;}

void* DnsWorker::main(){
    work();
    retval = 0;
    return &retval;
}

void DnsWorker::work(){
    char* const temp = new char[maxmessage];

    cout << "DnsWorker " << this->what() << " starting..." << endl;
    while (!stop_flag){
        try {
            try {
                size_t read = readQuery(temp, maxmessage);
                if (read == 0)
                    throw Socket::SocketException("Read 0 bytes");
                cerr << "\nRead " << read << " bytes: " << endl;
                DnsMessage query(temp, read);
                cout << "Query is " << query << endl;
                DnsResponse response(query, resolver, maxmessage);
                cout << "Response is " << response << endl;
                try {
                    size_t towrite = response.serialize(temp, maxmessage);
                    size_t written = sendResponse(temp, towrite);
                    cout << "\nSent " << written << " bytes: " << endl;
                } catch (DnsMessage::SerializeException& e) {
                    throw DnsMessage::DnsException(query.getID(), DnsErrorResponse::SERVER_FAILURE, e.what());
                }
            } catch (DnsMessage::DnsException& e){
                cerr << "Warning: message exception: " << e.what() << endl;
                DnsErrorResponse error_response(e);
                cerr << "Responding with " << error_response << endl;
                size_t towrite = error_response.serialize(temp, maxmessage);
                // hexdump(temp, towrite);
                size_t written = sendResponse(temp, towrite);
                cerr << "\nSent " << written << " bytes: " << endl;
            }
        } catch (DnsMessage::SerializeException& e) {
            cerr << "Warning: could not serialize error respose: " << e.what() << endl;
        } catch (Socket::SocketException& e) {
            cerr << "Warning: socket exception: " << e.what() << ". Continuing..." << endl;
        }
    }
    delete []temp;
}

int DnsWorker::uniqueid = 0;

// UdpWorker

UdpWorker::UdpWorker(DnsResolver& resolver, const UdpSocket& s, const size_t maxmessage) throw (Socket::SocketException)
    : DnsWorker(resolver, maxmessage), socket(s) {}

void UdpWorker::setup(){
    cerr << "UdpWorker starting up..." << endl;
} // Udp needs no special setup

size_t UdpWorker::readQuery(char* buff, const size_t maxmessage) throw (Socket::SocketException){
    return socket.recvfrom(buff, clientAddress, maxmessage);
}

size_t UdpWorker::sendResponse(const char* buff, const size_t maxmessage) throw (Socket::SocketException){
    return socket.sendto(buff, clientAddress, maxmessage);
}

string UdpWorker::what() const{
    stringstream ss;
    ss << "[UdpWorker: id = \'" << id << "\']";
    return ss.str();
}

// TcpWorker

TcpWorker::TcpWorker(DnsResolver& resolver, const TcpSocket& socket, Thread::Mutex& mutex, const size_t maxmessage)
    throw ()
    : DnsWorker(resolver, maxmessage),
      serverSocket(socket),
      acceptMutex(mutex)
{}

TcpWorker::~TcpWorker() throw () {
    connectedSocket->close();
    delete connectedSocket;
}

void TcpWorker::setup() throw (Socket::SocketException){
    cerr << "TcpWorker " << id << " starting up..." << endl;
    acceptMutex.lock();
    connectedSocket = serverSocket.accept();
    cerr << "TcpWorker " << id << " accepted connection..." << endl;
    acceptMutex.unlock();
} // Tcp needs no special setup

size_t TcpWorker::readQuery(char* buff, const size_t maxmessage) throw(Socket::SocketException){
    return connectedSocket->read(buff, maxmessage);
}

size_t TcpWorker::sendResponse(const char* buff, const size_t maxmessage) throw(Socket::SocketException){
    return connectedSocket->write(buff, maxmessage);
}

string TcpWorker::what() const{
    stringstream ss;
    ss << "[TcpWorker: id = \'" << id << "\']";
    return ss.str();
}


