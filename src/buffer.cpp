/**
 * @author See Contributors.txt for code contributors and overview of BadgerDB.
 *
 * @section LICENSE
 * Copyright (c) 2012 Database Group, Computer Sciences Department, University of Wisconsin-Madison.
 */

#include <memory>
#include <iostream>
#include "buffer.h"
#include "exceptions/buffer_exceeded_exception.h"
#include "exceptions/page_not_pinned_exception.h"
#include "exceptions/page_pinned_exception.h"
#include "exceptions/bad_buffer_exception.h"
#include "exceptions/hash_not_found_exception.h"

namespace badgerdb { 

BufMgr::BufMgr(std::uint32_t bufs)
	: numBufs(bufs) {
	bufDescTable = new BufDesc[bufs];

  for (FrameId i = 0; i < bufs; i++) 
  {
  	bufDescTable[i].frameNo = i;
  	bufDescTable[i].valid = false;
  }

  bufPool = new Page[bufs];

	int htsize = ((((int) (bufs * 1.2))*2)/2)+1;
  hashTable = new BufHashTbl (htsize);  // allocate the buffer hash table

  clockHand = bufs - 1;
}


BufMgr::~BufMgr() {
}

void BufMgr::advanceClock()
{
    clockHand=(clockHand+1)%numBufs;
}

void BufMgr::allocBuf(FrameId & frame) 
{
    //can go to infinite loop
    int i=0;
    bool found=false;
    while(i<=numBufs)
    {
        if(bufDescTable[clockHand].refbit==0 && bufDescTable[clockHand].pinCnt==0)
        {
            if(bufDescTable[clockHand].valid) {
                if (bufDescTable[clockHand].dirty) bufDescTable[clockHand].file->writePage(bufPool[clockHand]);
                hashTable->remove(bufDescTable[clockHand].file, bufDescTable[clockHand].pageNo);
            }
            found=true;
            frame=clockHand;
            break;
        }
        bufDescTable[clockHand].refbit=0;
        advanceClock();
        i++;
    }
    if(!found) { throw BufferExceededException(); }
}

	
void BufMgr::readPage(File* file, const PageId pageNo, Page*& page)
{
    try
    {
        FrameId frame;
        hashTable->lookup(file, pageNo, frame);
        page=&bufPool[frame];
        bufDescTable[frame].pinCnt++;
    }
    catch(HashNotFoundException e)
    {
        FrameId  frame;
        Page pageObj = file->readPage(pageNo);
        allocBuf(frame);
        bufPool[frame]=pageObj;
        page=&bufPool[frame];
        bufDescTable[frame].Set(file,pageNo);
        hashTable->insert(file,pageNo,frame);
    }

}


void BufMgr::unPinPage(File* file, const PageId pageNo, const bool dirty)
{
    try
    {
        FrameId frame;
        hashTable->lookup(file, pageNo, frame);
        if (bufDescTable[frame].pinCnt == 0) throw PageNotPinnedException(file->filename(), pageNo, frame);
        bufDescTable[frame].pinCnt--;
        if (dirty) bufDescTable[frame].dirty = true;
    }
    catch(HashNotFoundException e)
    {

    }
}

void BufMgr::flushFile(const File* file) 
{
    for(int i=0;i<numBufs;i++) {
        if(bufDescTable[i].file==file) {
            PageId pageNo=bufDescTable[i].pageNo;
            FrameId frame;
            hashTable->lookup(file,pageNo,frame);
            if(bufDescTable[i].pinCnt!=0) throw PagePinnedException(file->filename(),bufDescTable[i].pageNo,frame);
            if(bufDescTable[i].dirty) bufDescTable[i].file->writePage(bufPool[frame]);
            hashTable->remove(file,pageNo);
            bufDescTable[i].Clear();
        }
    }
}

void BufMgr::allocPage(File* file, PageId &pageNo, Page*& page) 
{
    Page pageObj = file->allocatePage();
    pageNo=pageObj.page_number();
    FrameId frame;
    allocBuf(frame);
    bufPool[frame]=pageObj;
    page=&bufPool[frame];
    bufDescTable[frame].Set(file,pageNo);
    hashTable->insert(file,pageNo,frame);
}

void BufMgr::disposePage(File* file, const PageId PageNo)
{
    try
    {
        FrameId frame;
        hashTable->lookup(file,PageNo,frame);
        hashTable->remove(file,PageNo);
        bufDescTable[frame].Clear();
    }
    catch (HashNotFoundException e)
    {
        file->deletePage(PageNo);
    }
}

void BufMgr::printSelf(void) 
{
  BufDesc* tmpbuf;
	int validFrames = 0;
  
  for (std::uint32_t i = 0; i < numBufs; i++)
	{
  	tmpbuf = &(bufDescTable[i]);
		std::cout << "FrameNo:" << i << " ";
		tmpbuf->Print();

  	if (tmpbuf->valid == true)
    	validFrames++;
  }

	std::cout << "Total Number of Valid Frames:" << validFrames << "\n";
}

}
