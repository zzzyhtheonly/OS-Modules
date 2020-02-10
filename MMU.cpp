#include <string>
#include <vector>
#include <memory>
#include <queue>
#include <list>
#include <cstdio>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

using namespace std;

enum Ins_type { c, r, w, e };

int ofs = 0;
vector<int> randvals;
int randSize;

static inline int myrandom(int burst) 
{
    if(ofs >= randSize)
		ofs = 0;
	return randvals[ofs++] % burst; 
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

struct PTE
{
    char VALID;
    char WRITE_PROTECT;
    char MODIFIED;
    char REFERENCED;
    char PAGEDOUT;
	char FILEMAPPED;
    unsigned short frame:7;
};

typedef struct
{
    int start;
    int end;
    char write_protected;
    char file_mapped;
}VMA;

struct Process
{
	unsigned long unmaps = 0;
    unsigned long maps = 0;
    unsigned long pstats = 0;
    unsigned long ins = 0;
    unsigned long outs = 0;
    unsigned long fins = 0;
    unsigned long fouts = 0;
    unsigned long zeros = 0;
    unsigned long segv = 0;
    unsigned long segprot = 0;

    vector<VMA*> vmas;
    int vmas_count = -1;
    int pid;
    VMA* vma_pages[64];
    PTE* page_table[64];
};


struct INS
{
    Ins_type type;
    int val;
};

struct Frame
{
    PTE* pte;
    int frame_id;
    int pid;
    int vpage;
};

int frame_count = -1;
vector<Frame*> frames;
int passed_ins = 0;

Process *CURRENT_PROCESS;
bool output_arg[8];
vector<INS> all_ins;
vector<Process*> procs;

unsigned long long cost = 0;
unsigned long ctx_switches = 0, inst_count = 0, process_exits = 0;
int free_page_idx = 0;
int proc_count = -1;

class Pager 
{
public:
    vector<Frame*> page_queue;
	vector<Frame*> exit_queue;
	int clock_count = 0;
    virtual Frame* choose_frame(){};
    virtual void init(){};
};

class FIFO:public Pager
{
public:
    void init();
    Frame* choose_frame();     
};

class Random:public Pager
{
public:
    Frame* choose_frame();    
};

class NRU:public Pager
{
public:
    Frame* choose_frame(); 
};

class Clock: public Pager
{
public:
    Frame* choose_frame();    

    unsigned int page_idx=0;
};

class Aging: public Pager
{
public:
	void init();
    Frame* choose_frame();

    vector<unsigned int>age_vector; 
};

class Working_Set: public Pager
{
public:
	vector<unsigned long> tau;
	void init();
	Frame* choose_frame();
};

void FIFO::init()
{
		for(auto i : frames)
			page_queue.push_back(i);
}

Frame* FIFO::choose_frame()
{
 
    Frame* f = NULL;
	if(exit_queue.empty())
	{
	    f = page_queue[0];
        page_queue.erase(page_queue.begin());
		page_queue.push_back(f);
	}
	else
	{
	    f = exit_queue[0];
		exit_queue.erase(exit_queue.begin());
	}
    return f;
}

Frame* Random::choose_frame()
{
    Frame *res;
    if(exit_queue.empty())
    {
        int idx = myrandom(frame_count);
	    res = frames[idx];
    }
	else
	{
	    res = exit_queue[0];
		exit_queue.erase(exit_queue.begin());
	}
	return res;
}

Frame* NRU::choose_frame()
{
    Frame *res = NULL;
    int class_pref = 4;
	int idx;
	if(exit_queue.empty())
	{
	    for(int i = clock_count;i < frame_count + clock_count;i++)
        {  
            int judge = 2 * frames[i % frame_count]->pte->REFERENCED + 1 * frames[i % frame_count]->pte->MODIFIED;	
		    if(judge < class_pref)
		    {
		        res = frames[i % frame_count];
			    idx = i % frame_count;
			    class_pref = judge;
		    }
        }
	    clock_count = (idx + 1) % frame_count;

	    if(passed_ins >= 50)
	    {
	        for(auto f : frames)
			    f->pte->REFERENCED = 0;
		    passed_ins = 0;
	    }
	}
	else
	{
	    res = exit_queue[0];
		exit_queue.erase(exit_queue.begin());
	}
	return res;
}

Frame* Clock::choose_frame()
{
	Frame *res = NULL;
    if(!exit_queue.empty())
    {
        res = exit_queue[0];
		exit_queue.erase(exit_queue.begin());
		return res;
    }
    while(1)
	{
	    if (page_idx >= frame_count) 
			page_idx = 0;
        res = frames[page_idx++];
        if(!res->pte->REFERENCED)
			return res;
        res->pte->REFERENCED = 0;
    }
}

void Aging::init()
{
    for(int i = 0;i < frame_count;i++)
		age_vector.push_back(0);
}

Frame* Aging::choose_frame()
{
    Frame *res = NULL;
	if(!exit_queue.empty())
	{
	    res = exit_queue[0];
		exit_queue.erase(exit_queue.begin());
		age_vector[res->frame_id] = 0;
	}
	else
	{
	    for(int i = 0;i < frame_count;i++)
	    {
	        age_vector[i] >>= 1;
	        if(frames[i]->pte->REFERENCED == 1)
	        {
	            age_vector[i] += 0x80000000;
				frames[i]->pte->REFERENCED = 0;
	        }
	    }

		unsigned int min = clock_count;
		for(int i = clock_count;i < frame_count + clock_count;i++)
		{
		    if(age_vector[min] > age_vector[i % frame_count])
		    {
		        min = i % frame_count;
		    }
		}

		res = frames[min];
		clock_count = (min + 1) % frame_count;
		age_vector[min] = 0;
	}
	return res;
}

void Working_Set :: init()
{
    for(int i = 0;i < frame_count;i++)
		tau.push_back(0);
}

Frame *Working_Set ::choose_frame()
{
    Frame *res = NULL;
	if(!exit_queue.empty())
	{
	    res = exit_queue[0];
		exit_queue.erase(exit_queue.begin());
	}
	else
	{
	    for(int i = clock_count;i < frame_count + clock_count;i++)
	    {
	        if(frames[i % frame_count]->pte->REFERENCED == 1)
	        {
	            frames[i % frame_count]->pte->REFERENCED = 0;
				tau[i % frame_count] = passed_ins;
	        }
			else
			{
			    if(passed_ins - tau[i % frame_count] >= 50)
			    {
			        clock_count = (i + 1) % frame_count;
					res = frames[i % frame_count];
					return res;
			    }
			}
	    }

		int min = clock_count;
		for(int i = clock_count;i < frame_count + clock_count;i++)
		{
		    if(tau[min] > tau[i % frame_count])
		    {
		        min = i % frame_count;
		    }
		}
		clock_count = (min + 1) % frame_count;
		res = frames[min];
	}
	return res;
}

Pager *THE_PAGER;

void add_VMA(Process *p,string line)
{
    VMA *v = new VMA();
	sscanf(line.c_str(), "%d%d%d%d", &v->start, &v->end, &v->write_protected, &v->file_mapped);
	
    p->vmas.push_back(v);
    for(int i = v->start;i <= v->end;i++)
	{
        p->vma_pages[i] = v;
        p->page_table[i]->WRITE_PROTECT = v->write_protected;
        p->page_table[i]->FILEMAPPED = v->file_mapped;
    }
}

void parseInput(string filename)
{
    ifstream ifile(filename);
	string line;
    bool flag = true;

    for(int i = 0;getline(ifile, line);)
	{
        if(line.empty() || line[0]=='#')
            continue;
        if(proc_count < 0)
        {
            proc_count = stoi(line,NULL,10);
			continue;
        }
		if(i < proc_count)
		{
            if(flag)
			{
			    flag = false;
                int n_vmas = stoi(line,NULL,10);
			    Process *p = new Process();
			    p->pid = i;
			    for(int i = 0; i < 64;i++)
			    {
                    p->page_table[i] = new PTE();
                    p->vma_pages[i] = NULL;
			    }
                procs.push_back(p);
                procs[i]->vmas_count = n_vmas;
             }
             else
			 {
                 add_VMA(procs[i],line);
                 if(procs[i]->vmas.size() >= procs[i]->vmas_count)
				 {
                     flag = true;
                     i++;
                 }
             }
        }
        else
		{   
            Ins_type t;
            switch(line[0])
			{
                case 'c':
                    t = c;
				    break;
                case 'r':
			        t = r;
                    break;
                case 'w':
					t = w;
					break;
				case 'e':
					t = e;
					break;
             }
			 INS ins;
			 ins.type = t;
		     ins.val = stoi(line.substr(1),NULL,10);
             all_ins.push_back(ins);
        }
   }
}

Frame* get_frame()
{
    Frame *res = NULL;
    if(free_page_idx < frame_count)
        res = frames[free_page_idx++];
    else 
		res = THE_PAGER->choose_frame();
    return res;
}

INS get_next_instruction()
{
    INS res;
	if(!all_ins.empty())
    {
        res = all_ins[0];
        all_ins.erase(all_ins.begin());
        return res;
    }
    return res;

}

void initialize()
{
    for(int i = 0;i < frame_count;i++)
	{
        Frame * f = new Frame();
		f->frame_id = i;
	    f->pid = -1;
		f->vpage = -1;
		frames.push_back(f);
    }
	THE_PAGER->init();
}

char to_c(Ins_type t)
{
    char res;
    switch(t)
	{
        case c:
            res = 'c';
			break;
        case r:
            res = 'r';
			break;
        case w:
            res = 'w';
			break;
		case e:
            res = 'e';
			break;
    }
	return res;
}


void simulation()
{
    bool out_flag = output_arg[0];
	INS inst;
	PTE *pte;
	Frame *frame;
	vector<string> out;
    for (passed_ins = 1;!all_ins.empty();passed_ins++) 
	{
        inst = get_next_instruction();

        if(out_flag) 
            printf("%lu: ==> %c %d\n",inst_count++,to_c(inst.type),inst.val);
        if(inst.type == c)
		{
		    ctx_switches++;
            cost += 121;
            CURRENT_PROCESS = procs[inst.val];
            continue;
        }
		if(inst.type == e)
		{
		    process_exits++;
			cost += 175;
			CURRENT_PROCESS = procs[inst.val];
			printf("EXIT current process %d\n", CURRENT_PROCESS->pid);
			for(int i = 0;i < 64;i++)
			{
			    CURRENT_PROCESS->page_table[i]->PAGEDOUT = 0;
				CURRENT_PROCESS->page_table[i]->REFERENCED = 0;
				if(CURRENT_PROCESS->page_table[i]->VALID == 1)
				{
				    CURRENT_PROCESS->page_table[i]->VALID = 0;
					printf(" UNMAP %d:%d\n", CURRENT_PROCESS->pid, i);
					CURRENT_PROCESS->unmaps++;
					cost += 400;
					if(frames[CURRENT_PROCESS->page_table[i]->frame]->pid != -1)
						THE_PAGER->exit_queue.push_back(frames[CURRENT_PROCESS->page_table[i]->frame]);
					frames[CURRENT_PROCESS->page_table[i]->frame]->pid = -1;
					if(CURRENT_PROCESS->page_table[i]->MODIFIED && CURRENT_PROCESS->page_table[i]->FILEMAPPED)
					{
					    if(out_flag)
                            cout<<" FOUT"<<endl;
                        procs[frame->pid]->fouts++;
                        cost += 2500;
					}
					CURRENT_PROCESS->page_table[i]->MODIFIED = 0;
				}
			}
			continue;
		}
        cost++;
		
        pte = CURRENT_PROCESS->page_table[inst.val]; 
        if (!pte->VALID && !(inst.val < 64 && CURRENT_PROCESS->vma_pages[inst.val] != NULL))
		{
		    CURRENT_PROCESS->segv++;
            cost += 240;
            if(out_flag)
                cout<<" SEGV"<<endl;
            continue;
        }

		if(!pte->VALID)
		{
            frame = get_frame();
            if(frame->pid >= 0)
			{    
                frame->pte->VALID = 0;
				procs[frame->pid]->unmaps++;
                cost += 400;
                if(out_flag)
					printf(" UNMAP %d:%d\n",frame->pid,frame->vpage);

                if(frame->pte->MODIFIED)
				{    
                    if(frame->pte->FILEMAPPED)
					{
					    procs[frame->pid]->fouts++;
                        cost += 2500;
                        if(out_flag)
                            cout<<" FOUT"<<endl;
                    }
                    else
					{
					    procs[frame->pid]->outs++;
                        cost += 3000;
                        if(out_flag)
                            cout<<" OUT"<<endl;
                        frame->pte->PAGEDOUT = 1;
                    }
                    
                    frame->pte->MODIFIED = 0;
                }
            }
            
            if(pte->FILEMAPPED)
			{
			    CURRENT_PROCESS->fins++;
                cost += 2500;
                if(out_flag)
                    cout<<" FIN"<<endl;
            }
            else if(pte->PAGEDOUT)
			{
                CURRENT_PROCESS->ins++;
                cost += 3000;
			    if(out_flag)
                    cout<<" IN"<<endl;
                
            }
            else
			{
			    CURRENT_PROCESS->zeros++;
                cost += 150;
                if(out_flag)
                    cout<<" ZERO"<<endl;
            }

            CURRENT_PROCESS->maps++;
            cost += 400;
            if(out_flag)
				printf(" MAP %d\n",frame->frame_id);
            frame->pid = CURRENT_PROCESS->pid;
            frame->vpage = inst.val;
            frame->pte = pte;
            pte->frame = frame->frame_id;
        }

        pte->REFERENCED = 1;
        pte->VALID = 1;
        if(inst.type == w && pte->WRITE_PROTECT)
		{
		    CURRENT_PROCESS->segprot++;
            cost += 300;
            if(out_flag)
                cout<<" SEGPROT"<<endl;
        }
		else if(inst.type == w && !pte->WRITE_PROTECT)
			pte->MODIFIED = 1;
    }
}

string PTE_out(struct PTE *p)
{
    if(p->VALID)
	{
	    string res;
        if(p->REFERENCED) 
			res += "R";
        else 
			res += "-";
        if(p->MODIFIED)
			res += "M";
        else 
			res += "-";
        if(p->PAGEDOUT)
			res += "S";
        else 
			res += "-";
		return res;
    }
    if(p->PAGEDOUT) 
		return "#";
    else 
		return "*";
}

void output()
{
    if(output_arg[1])
	{ 
        for(auto p:procs)
		{
           string s;
           for(int i = 0;i < 64;i++)
	       {
               if(p->page_table[i]->VALID)
                   s += to_string(i) + ':' + PTE_out(p->page_table[i]);
               else
                   s += PTE_out(p->page_table[i]);
                s += ' ';
            }

            cout<<"PT["<<p->pid<<"]: "<< s <<endl;
        }
    }
    if(output_arg[2])
	{ 
		string s = "FT: ";
		for(auto p : frames)
		{
			if(!p->pte)
				s += "* ";
			else
				s += to_string(p->pid) + ":" + to_string(p->vpage) + " ";    
		}
		cout<<s<<endl;
    }
    if(output_arg[3])
	{ 
        for(auto p:procs){
            printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
                p->pid,
                p->unmaps, p->maps, p->ins, p->outs,
                p->fins, p->fouts, p->zeros,
                p->segv, p->segprot);
        }
        printf("TOTALCOST %lu %lu %lu %llu\n",  inst_count, ctx_switches,process_exits,cost);
    }
}

int main(int argc, char **argv)
{
    char c;
    while ((c = getopt(argc, argv, "f:a:o:")) != EOF)
	{
        switch (c)
		{
		case 'f':
            frame_count = stoi(optarg,NULL,10);
            break;
		case 'a':
            switch (optarg[0])
			{
                case 'f':
                    THE_PAGER = new FIFO();
                    break;
                case 'r':
                    THE_PAGER = new Random();
                    break;
                case 'e':
                    THE_PAGER = new NRU();
                    break;
                case 'c':
                    THE_PAGER = new Clock();
                    break;
                case 'a':
                    THE_PAGER = new Aging();
                    break;
				case 'w':
                    THE_PAGER = new Working_Set();
                    break;
            }
            break;
        case 'o':
			string str = optarg;
			if(str.find("O") != string :: npos)
				output_arg[0] = true;
			else
				output_arg[0] = false;
			if(str.find("P") != string :: npos)
				output_arg[1] = true;
			else
				output_arg[1] = false;
		    if(str.find("F") != string :: npos)
				output_arg[2] = true;
			else
				output_arg[2] = false;
			if(str.find("S") != string :: npos)
				output_arg[3] = true;
			else
				output_arg[3] = false;
			if(str.find("x") != string :: npos)
				output_arg[4] = true;
			else
				output_arg[4] = false;
			if(str.find("y") != string :: npos)
				output_arg[5] = true;
			else
				output_arg[5] = false;
			if(str.find("f") != string :: npos)
				output_arg[6] = true;
			else
				output_arg[6] = false;
			if(str.find("a") != string :: npos)
				output_arg[7] = true;
			else
				output_arg[7] = false;
			break;
        }
    }

    initialize();
    parseInput(argv[argc - 2]);
	parseRandom(argv[argc - 1]);
    simulation();
    output();
    return 0;
}


