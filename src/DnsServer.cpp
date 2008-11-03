// TODO: install signal handler for the SIGTERM signal
// TODO: FATAL, WARNING, MESSAGE macros

// libc includes
#include <string.h>
#include <stdlib.h>

// stdl includes
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <list>

// project includes
#include "helper.h"
#include "DnsServer.h"

// usings
using namespace std;

// init of static consts
const char DnsServer::DEFAULT_HOSTS_FILE[MAX_FILE_NAME] = "/etc/hosts";
const unsigned int DnsServer::DEFAULT_UDP_PORT[3] = {53, 1, 65000};
const unsigned int DnsServer::DEFAULT_TCP_PORT[3] = {53, 1, 65000};
const unsigned int DnsServer::DEFAULT_UDP_WORKERS[3] = {1, 0, 50};
const unsigned int DnsServer::DEFAULT_TCP_WORKERS[3] = {5, 0, 50};
const unsigned int DnsServer::DEFAULT_TCP_TIMEOUT[3] = {2, 0, 300};

// class members definition

DnsServer::DnsServer (
    DnsResolver& resolver,
    const unsigned int udpport = DEFAULT_UDP_PORT[0],
    const unsigned int tcpport = DEFAULT_TCP_PORT[0],
    const unsigned int udpworkers = DEFAULT_UDP_WORKERS[0],
    const unsigned int tcpworkers = DEFAULT_TCP_WORKERS[0],
    const unsigned int tcptimeout = DEFAULT_TCP_TIMEOUT[0])
    throw(std::exception)
    {

        if (udpworkers > 0){
            udp_serversocket.bind_any(udpport);
        }

        if (tcpworkers > 0){
            int on = 1;
            tcp_serversocket.setsockopt (SOL_SOCKET, SO_REUSEADDR, (const char*) &on, sizeof (on));
            tcp_serversocket.bind_any(tcpport);
            tcp_serversocket.listen();
        }

        for (unsigned int i=0; i < udpworkers; i++)
            workers.push_back(new UdpWorker(resolver, udp_serversocket, resolve_mutex));

        for (unsigned int i=0; i < tcpworkers; i++)
            workers.push_back(new TcpWorker(resolver, tcp_serversocket, accept_mutex, resolve_mutex, tcptimeout));
    }

DnsServer::~DnsServer(){
    for (list<DnsWorker*>::iterator iter = workers.begin(); iter != workers.end(); iter++)
        delete *iter;
}

void DnsServer::start(){
    list<Thread> threads;

    cerr << "DnsServer creating worker threads..." << endl;
    for (list<DnsWorker*>::iterator iter = workers.begin(); iter != workers.end(); iter++){
        Thread t(**iter);
        threads.push_back(t);
    }

    cerr << "DnsServer running worker threads..." << endl;
    for (list<Thread>::iterator iter = threads.begin(); iter != threads.end(); iter++)
        iter->run();

    cerr << "DnsServer waiting for worker threads to finish..." << endl;
    for (list<Thread>::iterator iter = threads.begin(); iter != threads.end(); iter++)
    {
        int retval = 0;
        iter->join(&retval);
        cerr << "   DnsServer: Worker thread tid = " << iter->getTid() << " finished with retval " << retval << endl;
    }


    cerr << "DnsServer reports all threads joined. " << endl;
    udp_serversocket.close();
    tcp_serversocket.close();
    cerr << "DnsServer closing all serversockets. " << endl;
}
