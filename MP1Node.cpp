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
        memcpy((char *)(msg + 1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

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

void MP1Node::joinreq(Address *src_addr, void *data, size_t size)
{
    MessageHdr *msg;
    size_t msgSize = sizeof(MessageHdr) + sizeof(memberNode->addr) + sizeof(long) + 1;
    msg = (MessageHdr *)malloc(msgSize * sizeof(char));
    msg->msgType = JOINREP;

    memcpy((char *)(msg + 1), &memberNode->addr, sizeof(memberNode->addr));
    memcpy((char *)(msg + 1) + sizeof(memberNode->addr) + 1, &memberNode->heartbeat, sizeof(long));

    stringstream ss;
    ss << "Sending JOINREP To " << src_addr->getAddress() << " with heartbeat: " << memberNode->heartbeat;
    log->LOG(&memberNode->addr, ss.str().c_str());
    emulNet->ENsend(&memberNode->addr, src_addr, (char *)msg, msgSize);
    free(msg);
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

void MP1Node::sendRandomHB(Address *src_addr, long heartbeat)
{
    // send random heartbeat
    // send random heartbeat to few k members
    // randomly chosen by k
    int k = 50;
    double probability = k / (double)memberNode->memberList.size();
    MessageHdr *msg;

    size_t msgSize = sizeof(MessageHdr) + sizeof(memberNode->addr) + sizeof(long) + 1;
    msg = (MessageHdr *)malloc(msgSize * sizeof(char));
    msg->msgType = PING;
    memcpy((char *)(msg + 1), &memberNode->addr, sizeof(memberNode->addr));
    memcpy((char *)(msg + 1) + sizeof(memberNode->addr) + 1, &memberNode->heartbeat, sizeof(long));

    for (vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++)
    {
        Address dst_addr = mleAddress(&(*it));
        if ((dst_addr == memberNode->addr) || (dst_addr == *src_addr))
        {
            continue;
        }
        double randNum = ((double)(rand() % 100) / 100);
        if (randNum < probability)
        {
            emulNet->ENsend(&memberNode->addr, &dst_addr, (char *)msg, msgSize);
        }
        else
        {
            // log->LOG(&memberNode->addr, "Probability greater than rand");
        }
    }
    free(msg);
}

void MP1Node::pingHeartbeat(Address *addr, void *data, size_t size)
{
    long *heartbeat = (long *)data;
    bool isNewData = this->updateMemberList(addr, *heartbeat);
    if (isNewData)
    {
        // this->logMemberList();
        this->sendRandomHB(addr, *heartbeat);
    }
    else
    {
        // log->LOG(&memberNode->addr, "Heartbeat is up to date.");
    }
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

    MemberListEntry mle(*((int *)addr->addr), *((short *)&(addr->addr[4])), heartbeat, par->getcurrtime());
    memberNode->memberList.push_back(mle);
    log->logNodeAdd(&memberNode->addr, addr);
    return true;
}

void MP1Node::sendAliveReply(Address *src_addr, void *data, size_t size)
{
    MessageHdr* msg;
    Address *dst_addr = (Address *)(data);
    size_t msgSize = sizeof(MessageHdr) + sizeof(memberNode->addr) + sizeof(long) + 1;
    msg = (MessageHdr *)malloc(msgSize * sizeof(char));
    msg->msgType = ISALIVE;
    memcpy((char *)(msg+1), &memberNode->addr, sizeof(memberNode->addr));
    memcpy((char *)(msg+1) + sizeof(memberNode->addr) + 1, &memberNode->heartbeat, sizeof(long));
    emulNet->ENsend(&memberNode->addr, src_addr, (char *)msg, msgSize);
}

void MP1Node::checkIfAlive(Address *src_addr, void *data, size_t size)
{
    // msgtype
    MessageHdr *msg;
    Address *dst_addr = (Address *)(data);
    size_t msgSize = sizeof(MessageHdr) + sizeof(memberNode->addr) + 1;
    msg = (MessageHdr *)malloc(msgSize * sizeof(char));
    msg->msgType = CHECK;
    memcpy((char *)(msg + 1), &memberNode->addr, sizeof(memberNode->addr));
    emulNet->ENsend(&memberNode->addr, dst_addr, (char *)msg, msgSize);
    this->susTracker.push_back(make_pair(src_addr, dst_addr));
    free(msg);
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
    size -= sizeof(MessageHdr) + sizeof(Address) + 1;
    data += sizeof(MessageHdr) + sizeof(Address) + 1;
    if (msg->msgType == JOINREQ)
    {
        this->joinreq(src_addr, data, size);
    }
    else if (msg->msgType == PING)
    {
        this->pingHeartbeat(src_addr, data, size);
    }
    else if (msg->msgType == SUS)
    {
        this->checkIfAlive(src_addr, data, size);
    }
    else if (msg->msgType == CHECK)
    {
        // yet to implement
        this->sendAliveReply(src_addr, data, size);
    }
    else if (msg->msgType == ISALIVE)
    {
        // this->sendParticularHB();
    }
    else if (msg->msgType == JOINREP)
    {
        memberNode->inGroup = true;
        // stringstream ss;
        // ss << "JOINREP from: " << src_addr->getAddress();
        // ss << " msg " << *(long *)(data);
        // log->LOG(&memberNode->addr, ss.str().c_str());
    }
    else
    {
        log->LOG(&memberNode->addr, "NOT JOINREQ OR JOINREP");
        return false;
    }
    free(msg);
    return true;
}

void MP1Node::randomK(Address *dst_addr, int time)
{
    // send sus msg to random k nodes
    int k = 50;
    double probability = k / (double)memberNode->memberList.size();
    MessageHdr *msg;

    msg->msgType = SUS;
    size_t msgSize = sizeof(MessageHdr) + sizeof(memberNode->addr) + sizeof(memberNode->addr) + 1;
    memcpy((char *)(msg + 1), &memberNode->addr, sizeof(memberNode->addr));

    for (vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++)
    {
        Address addr = mleAddress(&(*it));
        if ((addr == memberNode->addr) || (*dst_addr == addr))
        {
            continue;
        }
        double randNum = ((double)(rand() % 100) / 100);
        if (randNum < probability)
        {
            memcpy((char *)(msg + 1) + sizeof(memberNode->addr) + 1, &addr, sizeof(memberNode->addr));
            emulNet->ENsend(&addr, dst_addr, (char *)msg, msgSize);
        }
        else
        {
            // log->LOG(&memberNode->addr, "Probability greater than rand");
        }
    }
    free(msg);
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
    int timeout = TREMOVE;
    stringstream ss;
    for (vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++)
    {
        if (par->getcurrtime() - it->timestamp > timeout)
        {
            Address addr = mleAddress(&(*it));
            ss << "Timing out " << addr.getAddress();

            // log->LOG(&memberNode->addr, ss.str().c_str());
            this->randomK(&addr, par->getcurrtime());
        }
        if (par->getcurrtime() - it->timestamp - TFAIL > timeout)
        {
            Address addr = mleAddress(&(*it));
            vector<MemberListEntry>::iterator next_node = it;
            vector<MemberListEntry>::iterator next_next_node = it + 1;
            for (next_node = it; next_next_node != memberNode->memberList.end(); next_node++, next_next_node++)
            {
                *next_node = *next_next_node;
            }

            memberNode->memberList.resize(memberNode->memberList.size() - 1);
            it--;
            // this->logMemberList();
            log->logNodeRemove(&memberNode->addr, &addr);
        }
    }
    if (par->getcurrtime() % 3 == 0)
    {
        this->updateMemberList(&memberNode->addr, memberNode->heartbeat);

        this->sendRandomHB(&memberNode->addr, memberNode->heartbeat);
    }
    memberNode->heartbeat++;
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
