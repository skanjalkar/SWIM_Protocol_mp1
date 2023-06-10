/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

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

void MP1Node::joinreq(Address *src_addr, void *data, size_t size)
{
    MessageHdr newMsg;
    newMsg.msgType = JOINREP;
    int id;
    short port;
    long heartbeat;
    long timestamp = par->getcurrtime();

    memcpy(&id, &src_addr[0], sizeof(int));
    memcpy(&port, &src_addr[4], sizeof(short));
    memcpy(&heartbeat, data + sizeof(MessageHdr) + sizeof(Address) + 1, sizeof(long));
    MemberListEntry newMember = MemberListEntry(id, port, heartbeat, timestamp);
    memberNode->memberList.push_back(newMember);

    int sizeOfVector = memberNode->memberList.size();
    size_t mlesize = sizeof(int) + sizeof(short) + 2 * sizeof(long);

    char *replyData = (char *)malloc(sizeof(MessageHdr) + sizeof(int) + (mlesize)*memberNode->memberList.size());
    memcpy(replyData, &newMsg.msgType, sizeof(MessageHdr));
    memcpy(replyData + sizeof(MessageHdr), &sizeOfVector, sizeof(int));
    char *pos = replyData + sizeof(MessageHdr) + sizeof(int);

    for (int i = 0; i < memberNode->memberList.size(); i++)
    {
        int toReplyId = memberNode->memberList[i].id;
        short toReplyPort = memberNode->memberList[i].port;
        long toReplyHeartBeat = memberNode->memberList[i].heartbeat;
        long toReplyTimeStamp = memberNode->memberList[i].timestamp;

        memcpy(pos, &toReplyId, sizeof(int));
        pos += sizeof(int);
        memcpy(pos, &toReplyPort, sizeof(short));
        pos += sizeof(short);
        memcpy(pos, &toReplyHeartBeat, sizeof(long));
        pos += sizeof(long);
        memcpy(pos, &toReplyTimeStamp, sizeof(long));
        pos += sizeof(long);
    }
    int totalSize = sizeof(MessageHdr) + sizeof(int) + mlesize * sizeOfVector;
    // cout << totalSize << endl;
    string ADDRESS = memberNode->addr.getAddress();
    log->LOG(&memberNode->addr, "SENT DATA FROM %d TO %d", memberNode->myPos, id);
    cout << endl;
    for (int i = 0; i < totalSize; i++)
    {
        printf("%02x", replyData[i]);
    }
    cout << endl;
    emulNet->ENsend(&memberNode->addr, src_addr, replyData, totalSize);
}

void MP1Node::joinHB(Address *addr, void *data, size_t size)
{
    size -= sizeof(MessageHdr) + sizeof(Address) + 1;
    long* heartbeat = (long*) data;

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
    // printf("\nMEMBER NODE %d :", memberNode->myPos);
    // for (int i = 0; i < size; i++)
    // {
    //     printf("%02x", data[i]);
    // }
    // cout << endl;
    MessageHdr *msg = (MessageHdr *)data;
    // cout << sizeof(msg->msgType) << endl;
    // cout << sizeof(*msg) << endl;
    // cout << "--------------------" << endl;
    // cout << msg->msgType << " : " << size << "MSG TYPE" <<  endl;

    if (msg->msgType == JOINREQ)
    {
        /*
        update current member list
        Reply with JOINREP, a vector of memberlist currently in the group
        */
        Address *src_addr = (Address *)(msg + 1);
        this->joinreq(src_addr, data, size);
        data += sizeof(MessageHdr) + sizeof(Address) + 1;
        this->joinHB(src_addr, data, size);
    }
    else if (msg->msgType == JOINREP)
    {
        /*
        update current member list
        ingroup = true
        */
        // cout << endl;
        // for (int i = 0; i < size; i++) {
        //     printf("%02x", data[i]);
        // }
        // cout << endl;
        int memberSize;
        memberNode->inGroup = true;
        memcpy(&memberSize, (data + sizeof(MessageHdr)), sizeof(int));
        // printf("%d", memberSize);
        char *pos = (char *)(data + sizeof(MessageHdr) + sizeof(int));


        for (int i = 0; i < memberSize; i++)
        {
            int recId;
            short recPort;
            long recHeartbeat;
            long recTimestamp;

            memcpy(&recId, pos, sizeof(int));
            pos += sizeof(int);
            memcpy(&recPort, pos, sizeof(short));
            pos += sizeof(short);
            memcpy(&recHeartbeat, pos, sizeof(long));
            pos += sizeof(long);
            memcpy(&recTimestamp, pos, sizeof(long));
            pos += sizeof(long);
            MemberListEntry newMember = MemberListEntry(recId, recPort, recHeartbeat, recTimestamp);
            memberNode->memberList.push_back(newMember);

            log->LOG(&memberNode->addr, "id, port, hb, timestamp: %02x %02x %02x %02x", recId, recPort, recHeartbeat, recTimestamp);
        }
        log->LOG(&memberNode->addr, "RECIEVED DATA FROM at %d", memberNode->myPos);
        for (vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++)
        {
            if (memberNode->addr == mleAddress(&(*it))) {
                memberNode->myPos = it;
            }
        }
    }
    else
    {
        log->LOG(&memberNode->addr, "NOT JOINREQ OR JOINREP");
        return false;
    }

    return true;
}

Address mleAddress(MemberListEntry* mle) {
    Address a;
    memcpy(a.addr, &mle->id, sizeof(int));
    memcpy(&a.addr[4], &mle->port, sizeof(short));
    return a;
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
    for (vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++) {
        if (par->getcurrtime() - it->timestamp > timeout) {
            Address addr = mleAddress(&(*it));
        }
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
