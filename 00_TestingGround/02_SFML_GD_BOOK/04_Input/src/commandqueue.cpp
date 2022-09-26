#include "commandqueue.h"

void
CommandQueue::push(const Command& command)
{
    mQueue.push_back(command);
}

Command 
CommandQueue::pop()
{
    Command& lastEle = mQueue.back();
    mQueue.pop_back();
    return lastEle;
 
}
    

bool
CommandQueue::isEmpty() const
{
    return mQueue.empty();
}
