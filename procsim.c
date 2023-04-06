#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

enum ProcessStatus {INIT,READY,RUNNING,IOBLOCKED,IOSERVICE,FINISHED};

enum Status {BUSY, IDLE};

enum Results {CONTINUE, GOTOIO, EXPIRED, ENDED, NONE};


// A process: data to be stored in Node
struct Process {
	// data provided in file
	char name[10];
	int totalCPUtime;
	double ioprobability;

	// statistics to report
	int wallclocktime;
	int givenCPU;
	int blockedIO;
	int timeIO;

	// simulation fields
	int timeCPULeft;
	int quantumleft;
	int ioblock;
	int timeIOleft;
	enum ProcessStatus status;
};

// The CPU
struct CPU {
	// statistics to report
	int timeBusy;
	int timeIdle;
	int numDispatches;
	
	// simulation fields
	struct Process* running_job;
	enum Status status;
	enum Results result;
	
};

// The I/O device
struct IODevice {
	// statistics to report
	int timeBusy;
	int timeIdle;
	int numDispatches;
	
	// simulation fields
	struct Process* io_job;
	enum Status status;
	enum Results result;
};

// A linked list (LL) node to store a queue entry
struct QNode {
	struct Process* job;
	struct QNode* next;
};

// The queue, front stores the front node of LL and rear stores the
// last node of LL
struct Queue {
	struct QNode *front, *rear;
};


// scheduler function pointer type definition
typedef   void (*CPUScheduler)(struct CPU* cpu, struct Queue* readyQ);

// The simulation structure
struct Simulation {
	// Wall clock
	int wallclock;
	
	// CPU
	struct CPU* cpu;
	// ready queue
	struct Queue* readyQ;
	// scheduler
	CPUScheduler scheduler;
	
	// IODevice
	struct IODevice* iodevice;
	// io blocked queue
	struct Queue* ioQ;	
	
	//Process lists
	struct Queue* FinishedJobs;
};

// A utility function to create a job
struct Process* newJob(char *p_name, int p_totalCPUtime, double p_ioprobability)
{
	struct Process* temp = (struct Process*)malloc(sizeof(struct Process));
	strcpy(temp->name, p_name);
	temp->totalCPUtime = p_totalCPUtime;
	temp->ioprobability = p_ioprobability;
	temp->timeCPULeft = p_totalCPUtime;
	temp->status = READY;
	temp->ioblock = 0; // Flag
	temp->givenCPU = 0;
	temp->blockedIO = 0; // counter
	temp->timeIO = 0;

	return temp;
}

// A utility function to create a CPU
struct CPU* createCPU()
{
	struct CPU* cpu = (struct CPU*)malloc(sizeof(struct CPU));
	cpu->running_job = NULL;
	cpu->status = IDLE;
	cpu->result = NONE;
	cpu->timeBusy = 0;
	cpu->timeIdle = 0;
	cpu->numDispatches = 0;

	return cpu;
}

// A utility function to create an I/O device
struct IODevice* createIODevice()
{
	struct IODevice* iodev = (struct IODevice*)malloc(sizeof(struct IODevice));
	iodev->io_job = NULL;
	iodev->status = IDLE;
	iodev->result = NONE;
	iodev->timeBusy = 0;
	iodev->timeIdle = 0;
	iodev->numDispatches = 0;
	
	return iodev;
}

// A utility function to create a new linked list node.
struct QNode* newNode(struct Process* p_job)
{
	struct QNode* temp = (struct QNode*)malloc(sizeof(struct QNode));
	temp->job = p_job;
	temp->next = NULL;
	return temp;
}

// A utility function to create an empty queue
struct Queue* createQueue()
{
	struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue));
	q->front = q->rear = NULL;
	return q;
}

// The function to add a job to q
void enQueue(struct Queue* q, struct Process* p_job)
{
	// Create a new LL node
	struct QNode* temp = newNode(p_job);

	// If queue is empty, then new node is front and rear both
	if (q->rear == NULL) {
		q->front = q->rear = temp;
		return;
	}

	// Add the new node at the end of queue and change rear
	q->rear->next = temp;
	q->rear = temp;
}

//Function to peek at the head of the queue q
struct Process* peek(struct Queue* q)
{
		return q->front->job;
}


// Function to remove the head from given queue q
struct Process* deQueue(struct Queue* q)
{
	// If queue is empty, return NULL.
	if (q->front == NULL)
		return NULL;

	// Store previous front and move front one node ahead
	struct QNode* temp = q->front;

	q->front = q->front->next;

	// If front becomes NULL, then change rear also as NULL
	if (q->front == NULL)
		q->rear = NULL;

	struct Process* topjob = temp->job;
	free(temp);
	
	return topjob;
}

// Check if the queue is empty
int isEmpty(struct Queue* q)
{
	return (int)(q->front == NULL);
}


/*
 * convert a string to a number
 * if an error occurs,, return -1 after setting *err to non-zero
 */
int tonum(char *str, int *err)
{
        int n = 0;      /* accumulates number */

        /* convert to number in the usal way */
        while(*str){
                if (isdigit(*str))      n = n * 10 + *str - '0';
                else{   /* oops . . . error */ *err = 1; return(-1); }
                str++;
        }
        /* done -- return integer */
        return(n);
}


// function to read input file.
struct Queue* readInputFile(char * filename)
{
	char name[100], runtime[100], probability[100];
	int err = 0;
	int iruntime = 0;
	double dprobability = 0.0;
	int linenum = 0;
	
	struct Queue* q = createQueue();
	
	FILE *fptr = fopen(filename, "r");
		
	if (fptr != NULL)
	{
		while(!feof(fptr))
		{
			linenum++;
			if (fscanf(fptr, "%s %s %s", name, runtime, probability) != 3)
			{
				/* the line in the input file is malformed */
				fprintf(stderr, "Malformed line %s(%d)\n", filename, linenum);
				exit(1);
			}
			if (strlen(name) > 10)
			{
				/* process name is to long (over 10 characters) */
				fprintf(stderr, "name is too long %s(%d)\n", filename, linenum);
				exit(1);
			}
			if ((iruntime = tonum(runtime,&err)) == (-1))
			{
				/* runtime is 0 or less */
				fprintf(stderr, "runtime is not positive integer %s(%d)\n", filename, linenum);
				exit(1);
			}
			dprobability=atof(probability);
			if (dprobability < 0.0 || dprobability > 1)
			{
				/* probability is not between 0 and 1 */
				fprintf(stderr, "probability < 0 or > 1 %s(%d)\n", filename, linenum);
				exit(1);
			}
			enQueue(q, newJob(filename, iruntime, dprobability));
		}
		fclose(fptr);
	}

	return q;
}
	
	
void fcfsCPU(struct CPU* cpu, struct Queue* readyQ) { // funtion for first come first serve cpu policy
	 // take first job in queue
	 struct Process* job = deQueue(readyQ);
	 
	 // check if we have more than 2 units to run
	 if (job->timeCPULeft >= 2)
	 {
		 // determine is we are going to have an IO block
		 if ((random() / RAND_MAX) < job->ioprobability)
		 {
			 //we have an io block, determine when
			 job->quantumleft = (random() % job->timeCPULeft) + 1;
			 job->ioblock = 1; // set flag
		 }
		 else
		 {
			 // no io block, just run until completion
			 job->quantumleft = job->timeCPULeft;
		 }
	 }
	 else
	 {
		 //just one time unit left to run 
		 job->quantumleft = job->timeCPULeft;
	 }
         job->status = RUNNING;
	 // do stats
	 job->givenCPU++;
	 
	 // put it in cpu
	 cpu->running_job = job;
	 cpu->status = BUSY;
	 cpu->numDispatches ++;

}


void rrCPU(struct CPU* cpu, struct Queue* readyQ) { // fucntion for round robin cpu policy
	 // take first job in queue
	 struct Process* job = deQueue(readyQ);
	 int N;
	 int quantum = 5;
	 
	 // check if we have more than 2 units to runn
	 if (job->timeCPULeft >= 2)
	 {
		 // determine is we are going to have an IO block
		 if ((random() / RAND_MAX) < job->ioprobability)
		 {
			 //we have an io block, determine when
			 N = ((job->timeCPULeft < quantum) ? job->timeCPULeft : quantum);
			 job->quantumleft = (random() % N) + 1;
			 job->ioblock = 1;
		 }
		 else
		 {
			 // no io block, just run for quantum or completion
			 N = ((job->timeCPULeft < quantum) ? job->timeCPULeft : quantum);
			 job->quantumleft = N;
		 }
	 }
	 else
	 {
		 //just one time unit left to run 
		 job->quantumleft = job->timeCPULeft;
	 }
         job->status = RUNNING;
	 // do stats
	 job->givenCPU++;
	 
	 // put it in cpu
	 cpu->running_job = job;
	 cpu->status = BUSY;
	 cpu->numDispatches ++;

}

void fcfsIO(struct IODevice* iodevice, struct Queue* ioQ) { // fucntion for first come first serve IO policy
	// take first job in queue
	struct Process* job = deQueue(ioQ);
	job->status = IOSERVICE;
	job->timeIOleft = (random() % 30) + 1;
	job->blockedIO++;

	// put it in IODevice
	iodevice->io_job = job;
	iodevice->status = BUSY;
	iodevice->numDispatches ++;
 
}


void dispatchToCPU(struct Simulation* s)
{
	(*(s->scheduler))(s->cpu, s->readyQ);
}

void dispatchToIO(struct Simulation* s)
{
	fcfsIO(s->iodevice, s->ioQ);
}

struct Process* runCPU(struct CPU* cpu) { // funtion for first come first serve cpu policy
	struct Process* job = NULL;
	
	if (cpu->status == BUSY)
	{
		//decrement quantum & time left in CPU
		cpu->running_job->quantumleft--;
		cpu->running_job->timeCPULeft--;
		// if quantum exppired, then handle it
		if (cpu->running_job->quantumleft == 0)
		{
			job = cpu->running_job;
			cpu->running_job = NULL;
			cpu->status = IDLE;
			// if time to block io 
			if (job->ioblock)
			{
				cpu->result = GOTOIO;
				job->status = IOBLOCKED;
				job->ioblock = 0;
			}
			// if execution ended
			else if (cpu->running_job->timeCPULeft == 0)
			{
				cpu->result = ENDED;
				job->status = FINISHED;
			}
			else // if quantum just ended, send it to ready queue
			{
				cpu->result = EXPIRED;
				job->status = READY;
			}
		}
		
	}
	
	return job;
	 
}


struct Process* runIO(struct IODevice* io) { // funtion for first come first serve cpu policy
	struct Process* job = NULL;
	
	if (io->status == BUSY)
	{
		//decrement time left in IO
		io->io_job->timeIOleft--;
		// if iotime expired, then handle it
		if (io->io_job->timeIOleft == 0)
		{
			job = io->io_job;
			job->status = READY;
			io->io_job = NULL;
			// time to IO ended, say so
			io->status = IDLE;
			io->result = ENDED;
		}
		
	}
	
	return job;
	 
}


struct Simulation* InitSimulation(char *filename, char scheduler_type)
{
	// create Simulation object
	struct Simulation* s = (struct Simulation*)malloc(sizeof(struct Simulation));
	
	// create CPU
	s->cpu = createCPU();
	
	//create ready list - read file
	s->readyQ = readInputFile(filename);
	
	// set scheduler type
	if (scheduler_type == 'f')
		s->scheduler = &fcfsCPU;
	else
		s->scheduler = &rrCPU;
	
	// create IODevice
	s->iodevice = createIODevice();
	
	// create ioblocked queue
	s->ioQ = createQueue();
	
	// create finished list
	s->FinishedJobs = createQueue();
	
	// init wall clock
	s->wallclock = 0;
	
    /* seed the PRNG */
    (void) srandom(12345);

	
	return s;
}

void Simulate(struct Simulation* s)
{
	// this happens in one wall clock unit.
	while(s->cpu->status == BUSY || s->iodevice->status == BUSY || !isEmpty(s->readyQ) || !isEmpty(s->ioQ))
	{
		// if cpu is IDLE try to maake it busy
		if (s->cpu->status == IDLE)
		{
				if  (!isEmpty(s->readyQ))
				{
					dispatchToCPU(s);
				}
		}
		
		//if io is IDLE, try to make it busy
		if (s->iodevice->status == IDLE)
		{
				if  (!isEmpty(s->ioQ))
				{
					dispatchToIO(s);
				}
		}
			
		// if cpu is busy, run the process,
		if (s->cpu->status == BUSY)
		{
			struct Process* job = runCPU(s->cpu);
			s->cpu->timeBusy++;
			if (job != NULL)
			{
				if (job->status ==  FINISHED)
				{
					job->wallclocktime = s->wallclock;
					enQueue(s->FinishedJobs, job);
				}
				else if (job->status ==  READY)
				{
					enQueue(s->readyQ, job);
				}
				else if (job->status == IOBLOCKED)
				{
					enQueue(s->ioQ, job);
				}
				else
				{
					// we have an error 
				}
			}
		}
		else
			s->cpu->timeIdle++;
		
		// if io is busy, do the process
		if (s->iodevice->status == BUSY)
		{
			
			struct Process* job = runIO(s->iodevice);
			s->iodevice->timeBusy++;
			if (job != NULL)
			{
				if (job->status ==  READY)
				{
					enQueue(s->readyQ, job);
				}
				else
				{
					// we have an error
				}
			}
		}
		else
			s->iodevice->timeIdle++;
			
		// advance clock
		s->wallclock++;
	}
}

void printSimulationStatus(struct Simulation* s)
{
	printf("\nSystem:\n");
	printf("Current wall clock: %d\n", s->wallclock);

	printf("\nCPU:\n");
	
	
}

void printResults(struct Simulation* s)
{
	int numjobs = 0;
	int iojobs = 0;
	/*
	 * printing process information
	 */
	//printf("Program output (to stdout):\n------------------\n");
	/* header line */
	printf("Processes:\n\n");
	
	// loop through the FinishedJobs queue
	printf("   name     CPU time  when done  cpu disp  i/o disp  i/o time\n");

	struct Process* job;
	while(!isEmpty(s->FinishedJobs))
	{
		job = deQueue(s->FinishedJobs);
		numjobs++;
		if (job->timeIO > 0)
			iojobs++;
		
		/* for process information */
		printf("%-10s %6d     %6d    %6d    %6d    %6d\n", job->name, job->totalCPUtime, job->wallclocktime, job->givenCPU, job->blockedIO, job->timeIO);

	}

	/* print clock time at end */
	printf("\nSystem:\n");
	printf("The wall clock time at which the simulation finished: %d\n", s->wallclock);

	/* print cpu statistics */
	printf("\nCPU:\n");
	printf("Total time spent busy: %d\n", s->cpu->timeBusy);
	printf("Total time spent idle: %d\n", s->cpu->timeIdle);
	printf("CPU utilization: %.2f\n", (((double)s->cpu->timeBusy)/((double)(s->cpu->timeBusy + s->cpu->timeIdle))));
	printf("Number of dispatches: %d\n", s->cpu->numDispatches);
	printf("Overall throughput: %.2f\n", ((double)numjobs)/((double)s->wallclock));
	
	/* print i/o statistics */
	printf("\nI/O device:\n");
	printf("Total time spent busy: %d\n", s->iodevice->timeBusy);
	printf("Total time spent idle: %d\n", s->iodevice->timeIdle);
	printf("I/O utilization: %.2f\n", ((double)s->iodevice->timeBusy)/((double)(s->iodevice->timeBusy + s->iodevice->timeIdle)));
	printf("Number of dispatches: %d\n", s->iodevice->numDispatches);
	printf("Overall throughput: %.2f\n", ((double)iojobs)/((double)s->wallclock));

}	


// validate input
int ValidateInput(char *filename, char *cpuschedtype, char *command)
{
	int retval = 0;
	if (cpuschedtype[0] == '-') 
	{
		if (cpuschedtype[1] == 'r')
		{
			retval =  1;
		}
		else
		{
			if (cpuschedtype[1] == 'f')
			{
				retval =  1;
			}
			else
			{
				retval = 0;
			}
		}
	}
	if (retval)
	{
		FILE *fptr = fopen(filename, "r");
		
		if (fptr == NULL)
		{
			/* can't open file */
			errno = ENOENT;
			perror(filename);
			retval = 0;
		}
		else
		{
			fclose(fptr);
		}
	}
	else
	{
		/* bad option (not -f or -r) */
		fprintf(stderr, "Usage: %s [-r | -f] file\n", command);
	}
	
	return retval;
}

void cleanup(struct Simulation* s)
{
	// queues, cpu and iodevice are empty, just finished jobs
	free(s->cpu);
	free(s->iodevice);
	free(s->readyQ);
	free(s->ioQ);
	while(!isEmpty(s->FinishedJobs))
	{
		struct Process* job = deQueue(s->FinishedJobs);
		free(job);
	}
	free(s->FinishedJobs);
}


// Driver Program 
int main(int argc, char *argv[])
{
	/*
	 * error messages
	 * the arguments for %s(%d) are file name and line number,
	 *     respectively
	 * all cause exit with status code 1 (ie, exit(1))
	 */
	//printf("\n\n\nProgram error output (to stderr):\n------------------\n");
	int retval = 0;
	if (argc == 3)
	{
		if (ValidateInput(argv[2],argv[1],argv[0]))
		{
			struct Simulation* s = InitSimulation(argv[2],argv[1][1]);
			Simulate(s);
			
			printResults(s);
			cleanup(s);
		}
		else
		{
			retval = 1;
		}
	}
	else
	{
		/* bad option (not -f or -r) */
		fprintf(stderr, "Usage: %s [-r | -f] file\n", argv[0]);
		retval = 1;
	}
	return retval;
}
