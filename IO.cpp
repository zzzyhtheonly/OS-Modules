#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <algorithm>

using namespace std;

int current_time = 0,current_track = 0, tot_movement = 0;

typedef struct
{
    int arrive_time;
	int track;

	int start_time;
	int end_time;
}IO_evt;

vector<IO_evt> all_IO; 

void parseInput(string filename)
{
    ifstream ifile(filename);
	string line;

    for(;getline(ifile, line);)
	{
        if(line.empty() || line[0]=='#')
            continue;
		IO_evt io;
		sscanf(line.c_str(), "%d%d", &io.arrive_time, &io.track);
		io.end_time = INT_MAX;
		all_IO.push_back(io);
   }
}

class scheduler
{
public:
	vector<int> IO_queue;

	virtual void init(){};
	virtual int choose_io(){};
};

class FIFO:public scheduler
{
public:
	void init();
	int choose_io();
};

class SSTF:public scheduler
{
public: 
	void init();
	int choose_io();
};

class LOOK:public scheduler
{
public:
	bool left = false;
	void init();
	int choose_io();
};

class CLOOK:public scheduler
{
public:
	int choose_io();
};

class FLOOK:public scheduler
{
public:
	bool left = false;
	vector<int> ac_q;
	void init();
	int choose_io();
};

void FIFO :: init()
{
    for(int i = 0;i < all_IO.size();i++)
		IO_queue.push_back(i);
}

int FIFO :: choose_io()
{
    if(IO_queue.empty())
		return -1;
    int res = IO_queue[0];
    IO_queue.erase(IO_queue.begin());
	return res;
}

void SSTF :: init()
{
    IO_queue.push_back(0);
}

int SSTF :: choose_io()
{
    int min = INT_MAX,idx = -1;
	bool flag = false;
    for(int i = 0;i < all_IO.size();i++)
    {
        if(all_IO[i].end_time > current_time)
        {
            flag = true;
            if(all_IO[i].arrive_time <= current_time)
            {
                int cost = abs(all_IO[i].track - current_track);
				if(cost < min)
				{
				    min = cost;
					idx = i;
				}
            }
        }
    }
	if(idx == -1 && flag)
	{
	    for(int i = 0;i < all_IO.size();i++)
	    {
	        if(all_IO[i].end_time > current_time)
	        {
	            return i;
	        }
	    }
	}
	
	return idx;
}

void LOOK :: init()
{
    
}

int LOOK :: choose_io()
{
    int min = INT_MAX,idx = -1;
	bool flag = false,reverse = false;
	lfind:
    for(int i = 0;i < all_IO.size();i++)
    {
        if(all_IO[i].end_time > current_time)
        {
            flag = true;
            if(all_IO[i].arrive_time <= current_time)
            {
                reverse = true;
                if(left && all_IO[i].track - current_track <= 0)
                {
                    int cost = abs(all_IO[i].track - current_track);
				    if(cost < min)
				    {
				        min = cost;
					    idx = i;
				    }
                }
				if(!left && all_IO[i].track - current_track >= 0)
				{
				    int cost = abs(all_IO[i].track - current_track);
				    if(cost < min)
				    {
				        min = cost;
					    idx = i;
				    }
				}
            }
        }
    }
	if(idx == -1 && reverse)
	{
	    left = !left;
		goto lfind;
	}
	if(idx == -1 && flag)
	{
	    for(int i = 0;i < all_IO.size();i++)
	    {
	        if(all_IO[i].end_time > current_time)
	        {
	            if(all_IO[i].track - current_track > 0)
					left = false;
				else if(all_IO[i].track - current_track < 0)
					left = true;
	            return i;
	        }
	    }
	}
	
	return idx;
}

int CLOOK :: choose_io()
{
    int min = INT_MAX,idx = -1;
	bool flag = false,reverse = false;
	
    for(int i = 0;i < all_IO.size();i++)
    {
        if(all_IO[i].end_time > current_time)
        {
            flag = true;
            if(all_IO[i].arrive_time <= current_time)
            {
                reverse = true;
				if(all_IO[i].track - current_track >= 0)
				{
				    int cost = abs(all_IO[i].track - current_track);
				    if(cost < min)
				    {
				        min = cost;
					    idx = i;
				    }
				}
            }
        }
    }
	if(idx == -1 && reverse)
	{
		int max = INT_MIN;
		for(int i = 0;i < all_IO.size();i++)
		{
		    if(all_IO[i].end_time > current_time)
		    {
		        if(all_IO[i].arrive_time <= current_time)
		        {
		            if(all_IO[i].track - current_track < 0)
		            {
		                int cost = abs(all_IO[i].track - current_track);
				        if(cost > max)
				        {
				            max = cost;
					        idx = i;
				        }
		            }
		        }
		    }
		}
	}
	if(idx == -1 && flag)
	{
	    for(int i = 0;i < all_IO.size();i++)
	    {
	        if(all_IO[i].end_time > current_time)
	        {
	            return i;
	        }
	    }
	}
	
	return idx;
}

void FLOOK :: init()
{
    IO_queue.push_back(0);
}

int FLOOK :: choose_io()
{
    int min = INT_MAX,idx = -1;
	bool flag = false,reverse = false;
	
	for(int i = 0;i < all_IO.size();i++)
    {
        if(all_IO[i].end_time > current_time)
        {
            flag = true;
            if(all_IO[i].arrive_time <= current_time)
            {
                if(find(IO_queue.begin(),IO_queue.end(),i) == IO_queue.end() && find(ac_q.begin(),ac_q.end(),i) == ac_q.end())
					IO_queue.push_back(i);
            }
        }
	}

	if(ac_q.empty())
		swap(ac_q,IO_queue);
	
	if(ac_q.empty() && flag)
	{
		for(int i = 0;i < all_IO.size();i++)
	    {
	        if(all_IO[i].end_time > current_time)
	        {
	            if(all_IO[i].track - current_track > 0)
					left = false;
				else if(all_IO[i].track - current_track < 0)
					left = true;
	            return i;
	        }
	    }
	}
	
	ffind:
    for(auto i: ac_q)
    {
        reverse = true;
        if(left && all_IO[i].track - current_track <= 0)
        {
            int cost = abs(all_IO[i].track - current_track);
		    if(cost < min)
		    {
				min = cost;
			    idx = i;
			}
        }
		if(!left && all_IO[i].track - current_track >= 0)
		{
		    int cost = abs(all_IO[i].track - current_track);
			if(cost < min)
		    {
				min = cost;
			    idx = i;
			}
		}
    }
	if(idx == -1 && reverse)
	{
	    left = !left;
		goto ffind;
	}
	for(int i = 0;i < ac_q.size();i++)
	{
	    if(ac_q[i] == idx)
	    {
	        ac_q.erase(ac_q.begin() + i);
			break;
	    }
	}
	return idx;
	
}


scheduler *The_scheduler;

void simulation()
{
    while(1)
    {
        int i = The_scheduler->choose_io();
		if(i == -1)
			break;
		
		current_time = max(current_time,all_IO[i].arrive_time);
		all_IO[i].start_time = current_time;
		int cost = abs(all_IO[i].track - current_track);
		tot_movement += cost;
		current_track = all_IO[i].track;
		current_time += cost;
		all_IO[i].end_time = current_time;
    }
}

void output()
{
	int total_time = 0;
	double avg_turnaround = 0, avg_waittime = 0;
	int max_waittime = 0;

    for(int i = 0;i < all_IO.size();i++)
    {
        printf("%5d: %5d %5d %5d\n", i, all_IO[i].arrive_time, all_IO[i].start_time, all_IO[i].end_time);
		total_time = max(total_time,all_IO[i].end_time);
		avg_turnaround += (all_IO[i].end_time - all_IO[i].arrive_time);
		avg_waittime += (all_IO[i].start_time - all_IO[i].arrive_time);
		max_waittime = max(max_waittime,all_IO[i].start_time - all_IO[i].arrive_time);
    }

	avg_turnaround /=  (double)all_IO.size();
	avg_waittime /=  (double)all_IO.size();
	
	printf("SUM: %d %d %.2lf %.2lf %d\n",
	 total_time, tot_movement, avg_turnaround, avg_waittime, max_waittime);
}

int main(int argc, char **argv)
{
    char c;
    while ((c = getopt(argc, argv, "s:")) != EOF)
	{
        switch (c)
		{
		    case 's':
			    switch (optarg[0])
			    {
                case 'i':
                    The_scheduler = new FIFO();
                    break;
                case 'j':
                    The_scheduler = new SSTF();
                    break;
                case 's':
                    The_scheduler = new LOOK();
                    break;
                case 'c':
                    The_scheduler = new CLOOK();
                    break;
                case 'f':
                    The_scheduler = new FLOOK();
                    break;
                }
                break;
			default:
			    break;
        }
    }

	parseInput(argv[argc - 1]);
	The_scheduler->init();
	simulation();
	output();
    return 0;
}



