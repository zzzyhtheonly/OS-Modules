#include <iostream>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdio>
#include <vector>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>


using namespace std;

int ofs = 0;
vector<int> randvals;
int randSize;

int CURRENT_TIME = 0;
bool CALL_SCHEDULER = false;
bool PREEMPT = false;

enum states
{
     TRANS_TO_CREATED, TRANS_TO_READY, TRANS_TO_RUN, TRANS_TO_BLOCK, TRANS_TO_PREEMPT
};

enum schedType
{
    FCFS, LCFS, SRTF, RR, PRIO, PREIO
};

enum Pstates
{
    CREATED ,READY, RUN, BLOCK, END
};

static inline int myrandom(int burst) 
{
    if(ofs >= randSize)
		ofs = 0;
	return 1 + (randvals[ofs++] % burst); 
}


void parseRandom(char * filename)
{
    ifstream ifile(filename);
	string s;
	getline(ifile,s);
	randSize = atoi(s.c_str());
	while(getline(ifile,s))
	    randvals.push_back(atoi(s.c_str()));	
}

class Process {
public:

	int PID;
	enum Pstates state;

	int remainTime, remainingCB, recentReady, startRun, endRun;

	int AT, TC, CB, IO, IT, CW, FT;
	int static_prio;
	int dynamic_prio;
};

vector<Process*> proc;
Process *CURRENT_RUNNING_PROCESS = NULL;

typedef struct 
{
	int timestamp;
	Process *evtP;
	enum states transition;
}Event;

vector<Event> EventQueue;

void put_event(int e, Process* p, enum states t) 
{
    Event evt;
	evt.evtP = p;
	evt.timestamp = e;
	evt.transition = t;
    int i = 0;
    for (; i < EventQueue.size(); i++) 
	{
       if (e < EventQueue[i].timestamp) 
           break;
	}
    EventQueue.insert(EventQueue.begin() + i, evt);
}

void rm_event(Process *p)
{
    for(int i = 0;i < EventQueue.size();i++)
    {
        if(p == EventQueue[i].evtP && (EventQueue[i].transition == TRANS_TO_RUN || EventQueue[i].transition == TRANS_TO_PREEMPT))
			EventQueue.erase(EventQueue.begin() + i--);
    }
}

class Scheduler {
public:
	int quantum;
	enum schedType type;
	vector<Process*> readyQueue;
	vector<Process*> activeQueue;
	vector<Process*> expireQueue;

    void add_process()
    {
        CURRENT_TIME = proc[0]->AT;
		for (int i = 0; i < proc.size(); i++) 
		    put_event(proc[i]->AT, proc[i], TRANS_TO_CREATED);
	}
	
	virtual Process* get_nextProcess() = 0;
	virtual void put_process(Process*) = 0;
	virtual bool isMax(Process*) = 0; 
};

Scheduler* THE_SCHEDULER;

class FCFSScheduler: public Scheduler {
public:
	FCFSScheduler(int q){quantum = q;};
	Process* get_nextProcess()
	{
		Process *p = readyQueue[0];
		readyQueue.erase(readyQueue.begin());
	    return p;
	};
	void put_process(Process* p)
	{
	    readyQueue.push_back(p);
	};
	bool isMax(Process *p){return true;};
};

class LCFSScheduler: public Scheduler {
public:
	LCFSScheduler(int q){quantum = q;};
	Process* get_nextProcess()
	{
		Process* p = readyQueue[readyQueue.size() - 1];
		readyQueue.pop_back();
	    return p;
	};
	void put_process(Process* p)
	{
	    readyQueue.push_back(p);
	};
	bool isMax(Process *p){return true;};
};

class SRTFScheduler: public Scheduler {
public:
    SRTFScheduler(int q){quantum = q;};
	Process* get_nextProcess()
	{
		int min = readyQueue[0]->remainTime;
		int index = 0;
		int i = 0;
		for (i++; i < readyQueue.size(); i++) 
		{
			if (readyQueue[i]->remainTime < min) 
			{
				min = readyQueue[i]->remainTime;
				index = i;
			}
		}
		Process* p = readyQueue[index];
		readyQueue.erase(readyQueue.begin() + index);
	    return p;
	};
	void put_process(Process* p)
	{
	    readyQueue.push_back(p);
	};
	bool isMax(Process *p){return true;};
};

class RRScheduler: public Scheduler {
public:
	RRScheduler(int q){quantum = q;};
	Process* get_nextProcess()
	{
		Process *p = readyQueue[0];
		readyQueue.erase(readyQueue.begin());
	    return p;
	};
	void put_process(Process* p)
	{
	    readyQueue.push_back(p);
	};
	bool isMax(Process *p){return true;};
};

class PRIOScheduler: public Scheduler {
public:
	PRIOScheduler(int q){quantum = q;};
	Process* get_nextProcess()
	{
	    if (activeQueue.size() == 0) 
	        swap(activeQueue,expireQueue);

	    int max = activeQueue[0]->dynamic_prio;
	    int i = 0,index = 0;
	    for (i++; i < activeQueue.size(); i++) 
	    {
		    if (activeQueue[i]->dynamic_prio > max) 
		    {
			    max = activeQueue[i]->dynamic_prio;
			    index = i;
		    }
	    }
	    Process *p = activeQueue[index];
	    activeQueue.erase(activeQueue.begin() + index);
	    return p;
	};
	void put_process(Process* p)
	{
		if (p->dynamic_prio == -1) 
	    {
		    p->dynamic_prio = p->static_prio - 1;
		    expireQueue.push_back(p);
	    } 
	    else
		    activeQueue.push_back(p);
	};
	bool isMax(Process *p){return true;};
};

class PREIOScheduler: public Scheduler {
public:
	PREIOScheduler(int q){quantum = q;};
	Process* get_nextProcess()
	{
	    if (activeQueue.size() == 0) 
	        swap(activeQueue,expireQueue);

	    int max = activeQueue[0]->dynamic_prio;
	    int i = 0,index = 0;
	    for (i++; i < activeQueue.size(); i++) 
	    {
		    if (activeQueue[i]->dynamic_prio > max) 
		    {
			    max = activeQueue[i]->dynamic_prio;
			    index = i;
		    }
	    }
	    Process *p = activeQueue[index];
	    activeQueue.erase(activeQueue.begin() + index);
	    return p;
	};
	void put_process(Process* p)
	{
		if (p->dynamic_prio == -1) 
	    {
		    p->dynamic_prio = p->static_prio - 1;
		    expireQueue.push_back(p);
	    } 
	    else
		    activeQueue.push_back(p);
	};
	bool isMax(Process *p)
	{
	    for (int i = 0; i < activeQueue.size(); i++) 
	    {
		    if (activeQueue[i]->dynamic_prio > p->dynamic_prio) 
				return false;
	    }
	    return true;
	};
};


void parseInput(char * filename) 
{
    ifstream ifile(filename);
	string str;
	int count = 0;
	int AT,TC,CB,IO;
    for(;getline(ifile,str);count++)
    {
    	Process* p = new Process();
		sscanf(str.c_str(), "%d%d%d%d", &AT, &TC, &CB, &IO);
		p->PID = count;
		p->AT = AT;
		p->remainTime = p->TC = TC;
		p->CB = CB;
		p->IO = IO;
		p->remainingCB = p->recentReady = p->FT = p->IT = p->CW = 0;
		p->static_prio = myrandom(4);
		p->dynamic_prio = p->static_prio - 1;
		proc.push_back(p);
	}
}

typedef struct WaitIO 
{
	int beg;
	int end;
} WaitIO;
bool compare(WaitIO a, WaitIO b)
{
	return a.beg == b.beg ? a.end < b.end : a.beg < b.beg;
}

vector<WaitIO> wait;

void Simulation()
{
	Event evt;
	Process *p;
	int timeToBlock;
	int lr = 0;
	
	while (!EventQueue.empty()) 
	{
	    evt = EventQueue[0];
		EventQueue.erase(EventQueue.begin());
		p = evt.evtP;
		CURRENT_TIME = max(CURRENT_TIME, evt.timestamp);
		//CUREENT_RUNNING_PROCESS = evt.timestamp < lr ? true:false;
		switch (evt.transition) 
		{ 
		    case TRANS_TO_BLOCK:
			    p->recentReady = CURRENT_TIME;
			    put_event(CURRENT_TIME, p, TRANS_TO_READY);
			    THE_SCHEDULER->put_process(p);
			    break;
			
		    case TRANS_TO_CREATED:
			    p->recentReady = p->AT;
			    put_event(p->AT, p, TRANS_TO_READY);
			    THE_SCHEDULER->put_process(p);
			    break;

		    case TRANS_TO_READY:
		    {
		        bool flag = false;
				int remain,end;
			    if (evt.timestamp < lr && !PREEMPT)
			    {
			  	    evt.timestamp = lr;
				    put_event(evt.timestamp,evt.evtP,evt.transition);
				    continue;
			    }
				else if(evt.timestamp < lr && PREEMPT && THE_SCHEDULER->isMax(CURRENT_RUNNING_PROCESS))
				{
					evt.timestamp = lr;
				    put_event(evt.timestamp,evt.evtP,evt.transition);
				    continue;
				}
				else if(evt.timestamp < lr && PREEMPT && (!THE_SCHEDULER->isMax(CURRENT_RUNNING_PROCESS) || evt.evtP->dynamic_prio > CURRENT_RUNNING_PROCESS->dynamic_prio))
				{
				    remain = CURRENT_RUNNING_PROCESS->endRun - CURRENT_TIME;
				    rm_event(CURRENT_RUNNING_PROCESS);
					flag = true;
				}
				p = THE_SCHEDULER->get_nextProcess();
			    p->CW += CURRENT_TIME - p->recentReady;

				if (p->remainingCB == 0) 
					p->remainingCB = myrandom(p->CB);
				
			    if (THE_SCHEDULER->quantum >= p->remainingCB) 
			    {
                    int tmp = p->remainTime;
					lr = min(CURRENT_TIME + p->remainTime, CURRENT_TIME + p->remainingCB);
					p->remainTime = max(0, p->remainTime - p->remainingCB);
					if(p->remainTime != 0)
					{
					    end = CURRENT_TIME + p->remainingCB;
					    put_event(CURRENT_TIME + p->remainingCB, p, TRANS_TO_RUN);
					}
					else
						end = p->FT = CURRENT_TIME + tmp;
					p->remainingCB = 0;
			    } 
			    else 
			    {					
				    int tmp = p->remainTime;
				    lr = min(CURRENT_TIME + THE_SCHEDULER->quantum, CURRENT_TIME + p->remainTime);
			        p->remainTime = max(0, p->remainTime - THE_SCHEDULER->quantum);
					if(p->remainTime != 0)
					{
					    end = CURRENT_TIME + THE_SCHEDULER->quantum;
					    put_event(CURRENT_TIME + THE_SCHEDULER->quantum, p, TRANS_TO_PREEMPT);
					}
					else
					    end = p->FT = CURRENT_TIME + tmp;
					p->remainingCB -= THE_SCHEDULER->quantum;
			    }
				p->startRun = CURRENT_TIME;
				p->endRun = end;
				if(flag)
				{
				    CURRENT_RUNNING_PROCESS->remainingCB += remain;
					CURRENT_RUNNING_PROCESS->remainTime += remain;
					//CURRENT_RUNNING_PROCESS->recentReady = CURRENT_TIME;
				    put_event(CURRENT_TIME, CURRENT_RUNNING_PROCESS, TRANS_TO_PREEMPT);
					//CURRENT_RUNNING_PROCESS->dynamic_prio--;
					//THE_SCHEDULER->put_process(CURRENT_RUNNING_PROCESS);
				}
				CURRENT_RUNNING_PROCESS = p;
			    break;
		    }
		    case TRANS_TO_RUN: 
			    timeToBlock = myrandom(p->IO);
			    WaitIO io;
			    io.beg = CURRENT_TIME;
			    io.end = CURRENT_TIME + timeToBlock;
			    wait.push_back(io);
			    p->IT += timeToBlock;
			    put_event(CURRENT_TIME + timeToBlock, p, TRANS_TO_BLOCK);
				p->dynamic_prio = p->static_prio - 1;
			    break;
			
		    case TRANS_TO_PREEMPT:
			    p->recentReady = CURRENT_TIME;
			    put_event(CURRENT_TIME, p, TRANS_TO_READY);
				p->dynamic_prio--;
			    THE_SCHEDULER->put_process(p);
			    break;
		}
	}
}

void output()
{
    int maxfintime = 0;
	double cpu_util = 0.0;
	double turnaround = 0.0;
	double io_util = 0.0;
	double waittime = 0.0;
	double Throughput = 0.0;

	sort(wait.begin(), wait.end(), compare);
	for (int end,i = 0; i < wait.size(); i++)
	{
		if (i == 0 || end <= wait[i].beg) 
		{
			io_util += wait[i].end - wait[i].beg;
			end = wait[i].end;
		} 
		else if (end < wait[i].end) 
		{
			io_util += wait[i].end - end;
			end = wait[i].end;
		}
	}

	for (int i = 0; i < proc.size(); i++) 
	{
		printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n", proc[i]->PID, proc[i]->AT,
				proc[i]->TC, proc[i]->CB, proc[i]->IO, proc[i]->static_prio, proc[i]->FT, proc[i]->FT - proc[i]->AT, proc[i]->IT, proc[i]->CW);
		maxfintime = max(proc[i]->FT,maxfintime);
		turnaround += proc[i]->FT - proc[i]->AT;
		waittime += proc[i]->CW;
		cpu_util += proc[i]->TC;
	}
	cpu_util *= (100 / (double)maxfintime);
	io_util *= (100 / (double)maxfintime);
	turnaround /= (double)proc.size();
	waittime /= (double)proc.size();
	Throughput = (double) proc.size() * 100 / maxfintime;
	printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n", maxfintime,
			cpu_util, io_util,
			turnaround, waittime, Throughput
			);
}

int main(int argc, char* argv[]) 
{
    char c;
	while((c = getopt(argc, argv, "vs:")) != EOF)
	{
		int quantum;
		switch (c)
		{
		case 's':
			if (optarg[0] == 'F') 
			{
			    cout<<"FCFS"<<endl;
			    THE_SCHEDULER = new FCFSScheduler(10000);
				THE_SCHEDULER->type = FCFS;
			}
			else if (optarg[0] == 'L')
			{
			    cout<<"LCFS"<<endl;
			    THE_SCHEDULER = new LCFSScheduler(10000);
				THE_SCHEDULER->type = LCFS;
			}
			else if (optarg[0] == 'S') 
			{
			    cout<<"SRTF"<<endl;
			    THE_SCHEDULER = new SRTFScheduler(10000);
				THE_SCHEDULER->type = SRTF;
			}
			else if (optarg[0] == 'R') 
			{
				sscanf(optarg + 1, "%d", &quantum);
				THE_SCHEDULER = new RRScheduler(quantum);
				THE_SCHEDULER->type = RR;
				cout<<"RR"<<" "<<quantum<<endl;
			}
			else if (optarg[0] == 'P') 
			{
				sscanf(optarg + 1, "%d", &quantum);
				THE_SCHEDULER = new PRIOScheduler(quantum);
				THE_SCHEDULER->type = PRIO;
				cout<<"PRIO"<<" "<<quantum<<endl;
			}
			else if(optarg[0] == 'E')
			{
			    sscanf(optarg + 1, "%d", &quantum);
				THE_SCHEDULER = new PREIOScheduler(quantum);
				THE_SCHEDULER->type = PREIO;
				PREEMPT = true;
				cout<<"PREPRIO"<<" "<<quantum<<endl;
			}
			break;
		}
	}
	parseRandom(argv[3]);
	parseInput(argv[2]);
	THE_SCHEDULER->add_process();
	Simulation();
	output();
	return 0;
}


