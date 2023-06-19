/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"
#include <sstream>

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address)
{
    for (int i = 0; i < 6; i++)
    {
        NULLADDR[i] = 0;
    }
    this->memberNode = member;
    this->emulNet = emul;
    this->log = log;
    this->par = params;
    this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop()
{
    if (memberNode->bFailed)
    {
        return false;
    }
    else
    {
        return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size)
{
    Queue q;
    return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport)
{
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if (initThisNode(&joinaddr) == -1)
    {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if (!introduceSelfToGroup(&joinaddr))
    {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr)
{
    /*
     * This function is partially implemented and may require changes
     */
    int id = *(int *)(&memberNode->addr.addr);
    int port = *(short *)(&memberNode->addr.addr[4]);

    memberNode->bFailed = false;
    memberNode->inited = true;
    memberNode->inGroup = false;
    // node is up!
    memberNode->nnb = 0;
    memberNode->heartbeat = 0;
    memberNode->pingCounter = TFAIL;
    memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr)
{
    MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if (0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr)))
    {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else
    {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *)malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg + 1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg + 1) + sizeof(memberNode->addr.addr) + 1, &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;
}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode()
{
    /*
     * Your code goes here
     */
    memberNode->inited = false;
    memberNode->inGroup = false;
    memberNode->nnb = 0;
    memberNode->heartbeat = 0;
    memberNode->pingCounter = TFAIL;
    memberNode->timeOutCounter = -1;
    memberNode->memberList.clear();
    return 0;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop()
{
    if (memberNode->bFailed)
    {
        return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if (!memberNode->inGroup)
    {
        return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages()
{
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while (!memberNode->mp1q.empty())
    {
        ptr = memberNode->mp1q.front().elt;
        size = memberNode->mp1q.front().size;
        memberNode->mp1q.pop();
        recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

Address mleAddress(MemberListEntry *mle)
{
    Address a;
    memcpy(a.addr, &mle->id, sizeof(int));
    memcpy(&a.addr[4], &mle->port, sizeof(short));
    return a;
}

void MP1Node::onJoinReq(Address *src_addr, void *data, size_t size)
{
    long heartbeat = *(long *)data;
    this->updateMemberList(src_addr, heartbeat);
    MsgTypes msg = JOINREP;
    auto serailizedMsg = this->serializeMSG(msg);
    int replySize = serailizedMsg.first;
    char *replyData = serailizedMsg.second;

    // stringstream ss;
    // ss << "Sending JOINREP To " << src_addr->getAddress() << " with heartbeat: " << memberNode->heartbeat;
    // log->LOG(&memberNode->addr, ss.str().c_str());
    emulNet->ENsend(&memberNode->addr, src_addr, replyData, replySize);
    free(replyData);
}

void MP1Node::onPing(Address *src_addr, void *data, size_t size)
{
    vector<MemberListEntry> newData = this->deserializePing((char *)data);

    for (int i = 0; i < newData.size(); i++)
    {
        Address addr = mleAddress(&newData[i]);
        auto it = this->susTracker.find(addr);
        if (it != this->susTracker.end())
        {
            for (int j = 0; j < this->susTracker[addr].size(); j++)
            {
                auto replyData = this->serializeMSG(MsgTypes::PING);
                this->emulNet->ENsend(&memberNode->addr, &this->susTracker[addr][j], replyData.second, replyData.first);
            }
            this->susTracker[addr].clear();
            this->susTracker.erase(it);
        }
        this->updateMemberList(&addr, newData[i].heartbeat);
    }
}

void MP1Node::logMemberList()
{
    stringstream ss;
    for (vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++)
    {
        ss << it->getid() << ": " << it->getheartbeat() << it->gettimestamp();
        " ), ";
    }
    log->LOG(&memberNode->addr, ss.str().c_str());
}

void MP1Node::serializeVector(char *buffer, vector<MemberListEntry> &src)
{
    int n = src.size();
    memcpy(buffer, &n, sizeof(int));
    buffer += sizeof(int);
    for (int i = 0; i < src.size(); i++)
    {
        memcpy(buffer, &src[i], sizeof(MemberListEntry));
        buffer += sizeof(MemberListEntry);
    }
}

pair<int, char *> MP1Node::serializeMSG(MsgTypes msgType)
{
    char *msg;
    int headerSize = sizeof(MessageHdr) + sizeof(Address);
    int totalsize;
    int n;
    int sizeOfVector;
    switch (msgType)
    {
    case JOINREQ:
        assert(false);
        break;
    case JOINREP:
    case PING:
        n = memberNode->memberList.size();
        sizeOfVector = n * sizeof(MemberListEntry);
        totalsize = headerSize + sizeof(int) + sizeOfVector;
        msg = (char *)malloc(totalsize * sizeof(char));
        this->serializeVector(msg + headerSize, this->memberNode->memberList);
        break;
    case SUS:
        n = this->mySusList.size();
        sizeOfVector = n * sizeof(MemberListEntry);
        totalsize = headerSize + sizeof(int) + sizeOfVector;
        msg = (char *)malloc(totalsize * sizeof(char));
        this->serializeVector(msg + headerSize, this->mySusList);
        break;
    case ISALIVE:
        totalsize = headerSize;
        msg = (char *)malloc(totalsize * sizeof(char));
        break;
    case DIS:
        totalsize = headerSize + sizeof(MemberListEntry);
        msg = (char *)malloc(totalsize * sizeof(char));
        memcpy(msg+headerSize, &memberNode->memberList.back(), sizeof(MemberListEntry));
        break;
    }

    char *starting = msg;
    MessageHdr pingType;
    pingType.msgType = msgType;
    memcpy(msg, &pingType, sizeof(MessageHdr));
    msg += sizeof(MessageHdr);
    memcpy(msg, &memberNode->addr, sizeof(Address));
    return make_pair(totalsize, starting);
}

vector<MemberListEntry> MP1Node::deserializePing(char *data)
{
    int size;
    memcpy(&size, data, sizeof(int));
    data += sizeof(int);
    vector<MemberListEntry> newData(size);
    for (int i = 0; i < size; i++)
    {
        memcpy(&newData[i], data, sizeof(MemberListEntry));
        data += sizeof(MemberListEntry);
    }
    return newData;
}

void MP1Node::sendMessageToKRand(MsgTypes msg)
{
    // send random heartbeat
    // send random heartbeat to few k members
    // randomly chosen by k
    int randNum = 50;
    auto replyData = this->serializeMSG(msg);

    int replySize = replyData.first;
    char *serilizedData = replyData.second;

    for (int i = 0; i < memberNode->memberList.size(); ++i)
    {
        int k = rand() % 100;
        Address dst_addr = mleAddress(&memberNode->memberList[i]);
        if (k < randNum)
        {
            emulNet->ENsend(&memberNode->addr, &dst_addr, serilizedData, replySize);
        }
        else
        {
            // log->LOG(&memberNode->addr, "Probability greater than rand");
        }
    }
    free(serilizedData);
}

bool MP1Node::updateMemberList(Address *addr, long heartbeat)
{
    for (vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++)
    {
        if (mleAddress(&(*it)) == *addr)
        {
            if (heartbeat > it->getheartbeat())
            {
                it->setheartbeat(heartbeat);
                it->settimestamp(par->getcurrtime());
                return true;
            }
            else
            {
                return false;
            }
        }
    }
    if (this->deadNodes.find(*addr) != this->deadNodes.end()) {
        return false;
    }
    MemberListEntry mle(*((int *)addr->addr), *((short *)&(addr->addr[4])), heartbeat, par->getcurrtime());
    memberNode->memberList.push_back(mle);
    // log->logNodeAdd(&memberNode->addr, addr);
    return true;
}

void MP1Node::onSus(Address *src_addr, void *data, size_t size)
{
    vector<MemberListEntry> incomingSusList = this->deserializePing((char *)data);
    for (int i = 0; i < incomingSusList.size(); i++)
    {
        Address sus_addr = mleAddress(&incomingSusList[i]);
        auto dummyMsg = this->serializeMSG(MsgTypes::ISALIVE);
        this->susTracker[sus_addr].push_back(*src_addr);
        emulNet->ENsend(&memberNode->addr, &sus_addr, dummyMsg.second, dummyMsg.first);
    }
}

void MP1Node::removeNode(Address* src_addr, void* data, size_t size) {
    MemberListEntry node;
    memcpy(&node, data, sizeof(MemberListEntry));
    Address addr = mleAddress(&node);

    // cout << memberNode->memberList.size() << "IN REMOVE NODE" << endl;
    for (int i = 0; i < memberNode->memberList.size(); i++) {
        if (memberNode->memberList[i].id == node.id && memberNode->memberList[i].port == node.port) {
            swap(memberNode->memberList[i], memberNode->memberList.back());
            this->sendMessageToKRand(MsgTypes::DIS);
            memberNode->memberList.pop_back();
            this->deadNodes.insert(addr);
            // log->logNodeRemove(&memberNode->addr, &addr);
            log->LOG(&memberNode->addr, "removed because of DIS msg");
            break;
        }
    }
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size)
{
    /*
     * Your code goes here
     */
    MessageHdr *msg = (MessageHdr *)data;
    Address *src_addr = (Address *)(msg + 1);
    size -= sizeof(MessageHdr) + sizeof(Address);
    data += sizeof(MessageHdr) + sizeof(Address);
    if (msg->msgType == JOINREQ)
    {
        this->onJoinReq(src_addr, data, size);
    }
    else if (msg->msgType == PING)
    {
        this->onPing(src_addr, data, size);
    }
    else if (msg->msgType == SUS)
    {
        this->onSus(src_addr, data, size);
    }
    else if (msg->msgType == ISALIVE)
    {
        auto replyData = this->serializeMSG(MsgTypes::PING);
        this->emulNet->ENsend(&memberNode->addr, src_addr, replyData.second, replyData.first);
    }
    else  if (msg->msgType == DIS) {
        this->removeNode(src_addr, data, size);
    }
    else if (msg->msgType == JOINREP)
    {
        memberNode->inGroup = true;
        this->onPing(src_addr, data, size);
    }
    else
    {
        log->LOG(&memberNode->addr, "NOT JOINREQ OR JOINREP");
        return false;
    }
    free(msg);
    return true;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps()
{

    /*
     * Your code goes here
     */
    // if (par->getcurrtime() == 490) {
    //     this->logMemberList();
    // }
    int timeout = TREMOVE;
    stringstream ss;
    for (int i = 0; i < memberNode->memberList.size(); i++)
    {
        MemberListEntry node = memberNode->memberList[i];
        if (par->getcurrtime() - node.timestamp > timeout)
        {
            this->mySusList.push_back(node);
        }

        if (par->getcurrtime() - node.timestamp - TFAIL > timeout)
        {
            Address addr = mleAddress(&node);
            swap(memberNode->memberList[i], memberNode->memberList[memberNode->memberList.size()-1]);
            this->sendMessageToKRand(MsgTypes::DIS);
            this->deadNodes.insert(addr);
            memberNode->memberList.pop_back();
            i--;
            // this->logMemberList();

            log->logNodeRemove(&memberNode->addr, &addr);
        }
    }
    if (this->mySusList.size() > 0)
    {
        this->sendMessageToKRand(MsgTypes::SUS);
        this->mySusList.clear();
    }

    memberNode->heartbeat++;
    if (memberNode->heartbeat % 3 == 0)
    {
        this->updateMemberList(&memberNode->addr, memberNode->heartbeat);
        this->sendMessageToKRand(MsgTypes::PING);
    }
    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr)
{
    return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress()
{
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode)
{
    memberNode->memberList.clear();
    int id = *(int *)(&memberNode->addr.addr);
    short port = *(short *)(&memberNode->addr.addr[4]);

    MemberListEntry mle(id, port);
    mle.settimestamp(par->getcurrtime());
    mle.setheartbeat(memberNode->heartbeat);
    memberNode->memberList.push_back(mle);
    return;
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n", addr->addr[0], addr->addr[1], addr->addr[2],
           addr->addr[3], *(short *)&addr->addr[4]);
}
