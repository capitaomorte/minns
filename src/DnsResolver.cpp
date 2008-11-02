// libc includes

#include <errno.h>
#include <string.h>

// stdl includes

#include <sstream>

// Project includes

#include "DnsResolver.h"

using namespace std;

DnsResolver::DnsResolver(
    const std::string& filename,
    const unsigned int maxsize,
    const unsigned int maxa,
    const unsigned int maxialiases) throw (ResolveException)
    : file(*new ifstream(filename.c_str(), ios::in)),
      maxaliases(maxa),
      cache(*new Cache(maxsize, maxialiases))
{
    if (file.fail())
        throw ResolveException(string("Could not open \'" + filename + "\'").c_str());
}


DnsResolver::~DnsResolver(){
    file.close();
    delete &file;
    delete &cache;
}

string& DnsResolver::resolve_to_string(const string& what) throw (ResolveException){
    static char buff[INET_ADDRSTRLEN];
    static string retval;
    stringstream ss;
    
    list<struct in_addr> result(*resolve(what));

    for (list<struct in_addr>::iterator iter = result.begin(); iter != result.end() ; iter++){
        struct in_addr temp = *iter;
        if (inet_ntop (
                AF_INET,
                reinterpret_cast<void *>(&temp),
                buff,
                sizeof(buff)) == NULL)
            throw ResolveException("Could not place network address in a string");
        ss << " " << buff;
    }
    retval.assign(ss.str());
    return retval;
}

const list<struct in_addr>* DnsResolver::resolve(const std::string& address) throw (ResolveException) {
    // lookup `address' in the cache
    list<struct in_addr>* result = cache.lookup(address);
    if (result != NULL) {
        cerr <<  "        (Cache HIT! for \'" << address << "\'\n";
        return result;
    }
    // cerr <<  "        (Cache MISS for \'" << address << "\')\n";

    // search the file
    file.clear();
    file.seekg(0, ios::beg);
    string line;
    while (getline(file, line)){
        // cerr << " got line \'" << line << "\'" << endl;
        DnsEntry parsed;
        if (parse_line(line, parsed) != -1){
            for (list<string>::iterator iter=parsed.aliases.begin(); iter != parsed.aliases.end(); iter++){
                if (address.compare(*iter) == 0){
                    // cerr << "        (File HIT for \'" << *iter << "\' inserted into cache )" << endl;
                    result = cache.insert(*iter, parsed.ip);
                } else if (!cache.full()) {
                    // cerr << "        (Inserting \'" << *iter << "\' into cache anyway )" << endl;
                    cache.insert(*iter, parsed.ip);
                } else {
                    // cerr << "        (Cache is full \'" << *iter << "\' not inserted)" << endl;
                }
            }
        }
    }
    if (result == NULL)
        throw ResolveException(string("Could not resolve \'" + address + "\'").c_str());
    return result;
}

int DnsResolver::parse_line(const std::string& line, DnsEntry& parsed) throw (ResolveException){
    if (line[0] == '#') return -1; // a # denotes a comment

    string buf;
    stringstream ss(line);

    if (ss >> buf){
        int retval = inet_pton(AF_INET, buf.c_str(), reinterpret_cast<char *>(&parsed.ip));
        switch (retval){
        case 0:
            return -1;
        case -1:
            throw ResolveException(errno, "Error in inet_pton()");
        default:
            break;
        }
    }  else return -1;

    while ((ss >> buf) and (parsed.aliases.size() < maxaliases))
        parsed.aliases.push_back(buf);

    return parsed.aliases.size();
}

std::ostream& operator<<(std::ostream& os, const DnsResolver& dns){
    return os <<
        "[DnsResolver: " <<
        "msize=\'" << dns.cache.local_map.size() << "\' " <<
        "lsize=\'" << dns.cache.local_list.size() << "\' " <<
        "head=\'" << dns.cache.print_head()  << "\' " <<
        "tail=\'" << dns.cache.print_tail()  << "\' " <<
        "]";
}


// Cache nested class

// MapValue constructor
DnsResolver::Cache::MapValue::MapValue(struct in_addr ip, list<map_t::iterator>::iterator li)
    : ips(1, ip), listiter(li) {}

DnsResolver::Cache::Cache(unsigned int ms, unsigned int maxialiases) :
    maxsize(ms),
    maxipaliases(maxialiases){}

DnsResolver::Cache::~Cache(){
    local_map.clear();
    local_list.clear();
}

list<struct in_addr> *DnsResolver::Cache::lookup(const string& name){
    // lookup the key in the map
    map_t::iterator i = local_map.find(name);
    if (i != local_map.end() ){
        // remove the iterator from the list pointing to the recently found
        // element. Do this with listiter!
        local_list.erase((i->second).listiter);
        // add it again at the front of the list
        local_list.push_front(i);
        return &((i->second).ips);
    }
    return NULL;
}

list<struct in_addr>* DnsResolver::Cache::insert(string& alias, struct in_addr ip){

    // add element to map and keep an iterator to it. point the newly
    // MapValue to local_list.begin(), but that will be made invalid soon.

    map_t::iterator retval_iter;
    if ((retval_iter = local_map.find(alias)) != local_map.end()){
        if (retval_iter->second.ips.size() < maxipaliases) 
            retval_iter->second.ips.push_back(ip);
    } else {
        MapValue value(ip, local_list.begin());
        pair<map_t::iterator,bool> temppair =
            local_map.insert(make_pair(alias, value));
        if (temppair.second != true){
            //cerr << "DnsResolver::Cache::insert(): Insertion of " << alias << " failed!" << endl;
            return NULL;
        }
        // insert into the beginning of the list the recently obtained iterator to
        // the map
        local_list.push_front(temppair.first);

        // now do the reverse: let the MapValue listiter element also point to its
        // place in the list
        temppair.first->second.listiter=local_list.begin();
        
        retval_iter = temppair.first;
        
        // if we exceeded the maximum allowed size, remove from both list and map
        // the least important element, the back of the list
        if (local_list.size() > maxsize){
            // remove from map
            cerr << "        (removing \'" << print_tail() << "\' from cache)" << endl;
            local_map.erase(local_list.back());
            local_list.pop_back();
        }
    }
    return &(retval_iter->second.ips);
}

bool DnsResolver::Cache::full() const {
    return local_list.size() == maxsize;
}

string DnsResolver::Cache::print_head() const {
    if (local_list.size() > 0)
        return (local_list.front())->first;
    else
        return "<none>";
}

string DnsResolver::Cache::print_tail() const {
    if (local_list.size() > 0)
        return (local_list.back())->first;
    else
        return "<none>";
}

// ResolveException nested class

DnsResolver::ResolveException::ResolveException(const char* s)
    : std::runtime_error(s) {
    errno_number = 0;
}

DnsResolver::ResolveException::ResolveException(int i, const char* s)
    : std::runtime_error(s), errno_number(i) {}

int DnsResolver::ResolveException::what_errno() const {return errno_number;}

const char * DnsResolver::ResolveException::what() const throw(){
    static string s;

    s.assign(std::runtime_error::what());

    if (errno_number != 0){
        char buff[MAXERRNOMSG] = {0};
        s += ": ";
        s.append(strerror_r(errno_number, buff, MAXERRNOMSG));
    }

    return s.c_str();
}

std::ostream& operator<<(std::ostream& os, const DnsResolver::ResolveException& e){
    return os << "[ResolveException: " << e.what() << "]";
}
