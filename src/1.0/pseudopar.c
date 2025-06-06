#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef long long int llint;

static char *
getNextWord (char *line, char *word)
{
	int wordlength = 0;

	while (((*line) == ' ') || ((*line) == '\t') || ((*line) == '\n') || ((*line) == '\0'))
	{
		++ line;
	}

	while (((*line) != ' ') && ((*line) != '\t') && ((*line) != '\n') && ((*line) != '\0'))
	{
		word[wordlength] = (*line);
		++ wordlength;
		++ line; 
	}
	
	word[wordlength] = '\0';
	return line;
}

double 
timer (void)
{
  struct rusage r;

  getrusage(0, &r);
  return (double) (r.ru_utime.tv_sec + r.ru_utime.tv_usec / (double)1000000);
}

struct node;

typedef struct arc 
{
  long long flow;
  long long capacity;
	long long wt;
	long long cst;
	int from;
	int to;
	char direction;
 // float *capacities;
} Arc;

typedef struct node
{
	Arc **outOfTree;
	Arc *arcToParent;
	struct node *parent;
	struct node *childList;
	struct node *nextScan;
	struct node *next;
	struct node *prev;
  long long excess;
  int numOutOfTree;
  int nextArc;
  int visited;
  int numAdjacent;
  int label;
  int breakpoint;
} Node;


typedef struct root 
{
	Node *start;
	Node *end;
} Root;

enum lambda_format {
	UNKNOWN,
	SEQUENCE,
	INTERVAL
};

//---------------  Global variables ------------------
static int numNodes = 0;
static int numArcs = 0;
static int source = 0;
static int sink = 1;
static int numParams = 0;

static int highestStrongLabel = 1;

static long long APP_VAL;

static Node *adjacencyList = NULL;
static Root *strongRoots = NULL;
static int *labelCount = NULL;
static Arc *arcList = NULL;

static long long*_params;

static long long init_param;
static long long end_param;
static long long step_param;
static long long curr_param;

static enum lambda_format lambda_format_input = UNKNOWN;

//-----------------------------------------------------

#ifdef STATS
static llint numPushes = 0;
static int numMergers = 0;
static int numRelabels = 0;
static int numGaps = 0;
static llint numArcScans = 0;
#endif

//#define USE_ARC_MACRO
#ifdef USE_ARC_MACRO

#define numax(a,b) (a>b ? a : b)

#define zero_floor(a) ( numax(a,0) )

// TODO: change conditions, they are too expensive
//#define computeArcCapacity(a,p) (a->from==(source) ?      \
																						zero_floor(a->wt-p*(a->cst)) :  \
																				(a->to==(sink) ? \
																					  zero_floor(p*(a->cst)-a->wt) :  \
																						a->capacity) )

#define computeArcCapacity(a,p) ( 																				 	\
		(a->from==(source))*(zero_floor(a->wt-p*(a->cst))) + 	 	\
		(a->to==(sink))*(zero_floor(p*(a->cst)-a->wt)) +         	\
		((a->from!=(source))&&(a->to!=(sink))) * 	\
				a->capacity																													\
		)

#else

static long long 
numax (long long a, long long b)
{
	return a>b ? a : b;
}

static long long
computeArcCapacity(Arc *ac, long long param)
{


#ifdef VERBOSE

	printf("c capacity of arc %d->%d with wt = %lld cst = %lld at param = %lld is %lld\n",
			ac->from,
			ac->to,
			ac->wt,
			ac->cst,
			param,
			numax( ac->wt + ((param*(ac->cst))/APP_VAL), 0) );
#endif
  return numax( ac->wt + ((param*(ac->cst))/APP_VAL), 0);
}


#endif

static int
isThereNextParam(int param)
{
	if ( lambda_format_input == SEQUENCE )
	{
		return param < numParams;
	}
	else if (lambda_format_input == INTERVAL)
	{
		return curr_param < end_param;

	}
	else
	{
		printf("c error : unset format type\n");
	}
	return 0;

}

static long long
getParameter (int param)
{
	if ( lambda_format_input == SEQUENCE )
	{
		return _params[param];
	}
	else if (lambda_format_input == INTERVAL )
	{
		curr_param = init_param + ((long long) param)* step_param;
		
		return curr_param < end_param ? curr_param : end_param;

	}

	return 0;
}

static void
initializeNode (Node *nd, const int n)
{
	nd->label = 0;
	nd->excess = 0;
	nd->parent = NULL;
	nd->childList = NULL;
	nd->nextScan = NULL;
	nd->nextArc = 0;
	nd->numOutOfTree = 0;
	nd->arcToParent = NULL;
	nd->next = NULL;
	nd->prev = NULL;
	nd->visited = 0;
	nd->numAdjacent = 0;
	//nd ->number = n;
	nd->outOfTree = NULL;
	nd->breakpoint = (numParams+1);
}

static void
initializeRoot (Root *rt) 
{
	rt->start = (Node *) malloc (sizeof(Node));
	rt->end = (Node *) malloc (sizeof(Node));

	if ((rt->start == NULL) || (rt->end == NULL))
	{
		printf ("%s Line %d: Out of memory\n", __FILE__, __LINE__);
		exit (1);
	}

	initializeNode (rt->start, 0);
	initializeNode (rt->end, 0);

	rt->start->next = rt->end;
	rt->end->prev = rt->start;
}


static void
freeRoot (Root *rt) 
{
	free(rt->start);
	rt->start = NULL;

	free(rt->end);
	rt->end = NULL;
}

static void
liftAll (Node *rootNode, const int theparam) 
{
	Node *temp, *current=rootNode;

	current->nextScan = current->childList;

	-- labelCount[current->label];
	current->label = numNodes;	
	current->breakpoint = (theparam+1);

	for ( ; (current); current = current->parent)
	{
		while (current->nextScan) 
		{
			temp = current->nextScan;
			current->nextScan = current->nextScan->next;
			current = temp;
			current->nextScan = current->childList;

			-- labelCount[current->label];
			current->label = numNodes;
			current->breakpoint = (theparam+1);	
		}
	}
}

static void
addToStrongBucket (Node *newRoot, Node *rootEnd) 
{
	newRoot->next = rootEnd;
	newRoot->prev = rootEnd->prev;
	rootEnd->prev = newRoot;
	newRoot->prev->next = newRoot;
}

static void
createOutOfTree (Node *nd)
{
	if (nd->numAdjacent)
	{
		if ((nd->outOfTree = (Arc **) malloc (nd->numAdjacent * sizeof (Arc *))) == NULL)
		{
			printf ("%s Line %d: Out of memory\n", __FILE__, __LINE__);
			exit (1);
		}
	}
}

static void
initializeArc (Arc *ac)
{
	ac->from = 0;
	ac->to = 0;
	ac->capacity = 0;
	ac->flow = 0;
	ac->direction = 1;
	//ac->capacities = NULL;
}

static void
addOutOfTreeNode (Node *n, Arc *out) 
{
	n->outOfTree[n->numOutOfTree] = out;
	++ n->numOutOfTree;
}

static void
readDimacsFileCreateList (void) 
{
	int lineLength=65536, i, capacity, numLines = 0, from, to, first=0, j, prec=1;
	int num_params;
	char *line, *word, ch, ch1, *tmpline;
	double param, a_i, b_i, init_par, end_par, step_par;
	Arc *ac = NULL;

	if ((line = (char *) malloc ((lineLength+1) * sizeof (char))) == NULL)
	{
		printf ("%s, %d: Could not allocate memory.\n", __FILE__, __LINE__);
		exit (1);
	}

	if ((word = (char *) malloc ((lineLength+1) * sizeof (char))) == NULL)
	{
		printf ("%s, %d: Could not allocate memory.\n", __FILE__, __LINE__);
		exit (1);
	}

	while (fgets (line, lineLength, stdin))
	{
		++ numLines;

#ifdef VERBOSE
		printf ("Read line %s\n", line);
#endif

		switch (*line)
		{
		case 'p':

			//sscanf (line, "%c %s %d %d %d", &ch, word, &numNodes, &numArcs, &numParams);
			tmpline = line;

			//ch
			tmpline = getNextWord(tmpline, word);
			//word
			tmpline = getNextWord(tmpline, word);

			if ( strcmp(word, "sequence" ) == 0 )
			{
				lambda_format_input = SEQUENCE;
			}
			else if ( strcmp(word, "interval" ) == 0 )
			{
				lambda_format_input = INTERVAL;
			}
			else
			{
				printf("c Unknown lambda input type %s", word);
				printf("c Options are \"interval\" and \"sequence\"\n");
				exit(0);
			}

			//numNodes 
			tmpline = getNextWord(tmpline, word);
			numNodes = atoi(word);

			//numArcs
			tmpline = getNextWord(tmpline, word);
			numArcs = atoi(word);

			//prec
			tmpline = getNextWord(tmpline, word);
			prec = atoi(word);
			APP_VAL = llround(pow(10.0, prec));

			// A bit messy, maybe worth refactoring this part


			if ( lambda_format_input == SEQUENCE)
			{
				tmpline = getNextWord(tmpline, word);
				num_params = atoi(word);

				
				if ((_params = (long long *) malloc ((num_params) * sizeof (num_params))) == NULL)
				{
					printf ("%s, %d: Could not allocate memory.\n", __FILE__, __LINE__);
					exit (1);
				}

				for ( i=0; i<num_params; ++i )
				{

					tmpline = getNextWord(tmpline, word);
					param = atof(word);
					_params[i] = llround(param* APP_VAL);

				}
				numParams = num_params;
				curr_param = _params[0];


			}
			else if (lambda_format_input == INTERVAL )
			{
				//init_param
				tmpline = getNextWord(tmpline, word);
				init_par = atof(word);
				init_param = llround(init_par * APP_VAL);

				//end_param
				tmpline = getNextWord(tmpline, word);
				end_par = atof(word);
				end_param = llround(end_par * APP_VAL);

				//step_param
				tmpline = getNextWord(tmpline, word);
				step_par = atof(word);
				step_param = llround(step_par * APP_VAL);

				curr_param = init_param;
			}



#ifdef VERBOSE
			printf ("numNodes: %d, numArcs: %d, numParams: %d, prec = %d\n", 
				numNodes,
				numArcs,
				numParams,
				prec);
#endif

			if ((adjacencyList = (Node *) malloc (numNodes * sizeof (Node))) == NULL)
			{
				printf ("%s, %d: Could not allocate memory.\n", __FILE__, __LINE__);
				exit (1);
			}

			if ((strongRoots = (Root *) malloc (numNodes * sizeof (Root))) == NULL)
			{
				printf ("%s, %d: Could not allocate memory.\n", __FILE__, __LINE__);
				exit (1);
			}

			if ((labelCount = (int *) malloc (numNodes * sizeof (int))) == NULL)
			{
				printf ("%s, %d: Could not allocate memory.\n", __FILE__, __LINE__);
				exit (1);
			}

			if ((arcList = (Arc *) malloc (numArcs * sizeof (Arc))) == NULL)
			{
				printf ("%s, %d: Could not allocate memory.\n", __FILE__, __LINE__);
				exit (1);
			}


			for (i=0; i<numNodes; ++i)
			{
				initializeRoot (&strongRoots[i]);
				initializeNode (&adjacencyList[i], (i+1));
				labelCount[i] = 0;
			}

			for (i=0; i<numArcs; ++i)
			{
				initializeArc (&arcList[i]);
			}




			break;

		case 'a':

			tmpline = line;
			++ tmpline;

			tmpline = getNextWord (tmpline, word);			
			from = (int) atoi (word);

			tmpline = getNextWord (tmpline, word);			
			to = (int) atoi (word);

			ac = &arcList[first];

			ac->from = from-1;
			ac->to = to-1;

			a_i = b_i = 0.0;

			tmpline = getNextWord (tmpline, word);			
			a_i = atof (word);

			if ( ac->from == source || ac->to == sink )
			{
#ifdef VERBOSE
				puts("c this arc goes from source or to sink");
#endif
				tmpline = getNextWord (tmpline, word);			
				b_i = atof (word);
			}
			else
			{
				b_i = a_i;
				a_i = 0;
			}

			ac->wt = llround(b_i * APP_VAL);
			ac->cst = llround(a_i* APP_VAL);


			// READ CAPACITY
			ac->capacity = computeArcCapacity(ac, curr_param);
				//numax(0, ((_params[0]*ac->cst)/APP_VAL) + ac->wt) ;

			++ first;
#ifdef VERBOSE
			printf("read arc %d->%d with wt = %lld cst = %lld, integer cap is  %lld\n",
					ac->from,
					ac->to,
					ac->wt,
					ac->cst,
					ac->capacity);
#endif
			++ adjacencyList[ac->from].numAdjacent;
			++ adjacencyList[ac->to].numAdjacent;

			break;

		case 'n':

			sscanf (line, "%c %d %c", &ch, &i, &ch1);

			if (ch1 == 's')
			{
				source = i-1;	
			}
			else if (ch1 == 't')
			{
				sink = i-1;	
			}
			else
			{
				printf ("Unrecognized character %c on line %d\n", ch1, numLines);
				exit (1);
			}

			break;
		}
	}

	for (i=0; i<numNodes; ++i) 
	{
		createOutOfTree (&adjacencyList[i]);
	}

	for (i=0; i<numArcs; i++) 
	{
		to = arcList[i].to;
		from = arcList[i].from;
		capacity = arcList[i].capacity;

		if (!((source == to) || (sink == from) || (from == to))) 
		{
			if ((source == from) && (to == sink)) 
			{
				arcList[i].flow = capacity;
			}
			else if (from == source)
			{
				addOutOfTreeNode (&adjacencyList[from], &arcList[i]);
			}
			else if (to == sink)
			{
				addOutOfTreeNode (&adjacencyList[to], &arcList[i]);
			}
			else
			{
				addOutOfTreeNode (&adjacencyList[from], &arcList[i]);
			}
		}
	}

	free (line);
	line = NULL;

	free (word);
	word = NULL;
}

static void
simpleInitialization (void) 
{
	int i, size;
	Arc *tempArc;

    size = adjacencyList[source].numOutOfTree;
    for (i=0; i<size; ++i)
	{
		tempArc = adjacencyList[source].outOfTree[i];
		tempArc->flow = tempArc->capacity;
		adjacencyList[tempArc->to].excess += tempArc->capacity;
	}

	size = adjacencyList[sink].numOutOfTree;
    for (i=0; i<size; ++i)
	{
		tempArc = adjacencyList[sink].outOfTree[i];
		tempArc->flow = tempArc->capacity;
		adjacencyList[tempArc->from].excess -= tempArc->capacity;
	}

	adjacencyList[source].excess = 0;
	adjacencyList[sink].excess = 0;

	for (i=0; i<numNodes; ++i) 
	{
		if (adjacencyList[i].excess > 0) 
		{
		    adjacencyList[i].label = 1;
			++ labelCount[1];

			addToStrongBucket (&adjacencyList[i], strongRoots[1].end);
		}
	}

	adjacencyList[source].label = numNodes;
	adjacencyList[source].breakpoint = 0;
	adjacencyList[sink].label = 0;
	adjacencyList[sink].breakpoint = (numParams+2);
	labelCount[0] = (numNodes - 2) - labelCount[1];
}

static inline int 
addRelationship (Node *newParent, Node *child) 
{
	child->parent = newParent;
	child->next = newParent->childList;
	newParent->childList = child;

	return 0;
}

static inline void
breakRelationship (Node *oldParent, Node *child) 
{
	Node *current;

	child->parent = NULL;

	if (oldParent->childList == child) 
	{
		oldParent->childList = child->next;
		child->next = NULL;
		return;
	}

	for (current = oldParent->childList; (current->next != child); current = current->next);

	current->next = child->next;
	child->next = NULL;
}

static void
merge (Node *parent, Node *child, Arc *newArc) 
{
	Arc *oldArc;
	Node *current = child, *oldParent, *newParent = parent;

#ifdef STATS
	++ numMergers;
#endif

	while (current->parent) 
	{
		oldArc = current->arcToParent;
		current->arcToParent = newArc;
		oldParent = current->parent;
		breakRelationship (oldParent, current);
		addRelationship (newParent, current);
		newParent = current;
		current = oldParent;
		newArc = oldArc;
		newArc->direction = 1 - newArc->direction;
	}

	current->arcToParent = newArc;
	addRelationship (newParent, current);
}


static inline void
//pushUpward (Arc *currentArc, Node *child, Node *parent, const int resCap)
pushUpward (Arc *currentArc, Node *child, Node *parent, const long long resCap)
{
#ifdef STATS
	++ numPushes;
#endif

	if (resCap >= child->excess) 
	{
		parent->excess += child->excess;
		currentArc->flow += child->excess;
		child->excess = 0;
		return;
	}

	currentArc->direction = 0;
	parent->excess += resCap;
	child->excess -= resCap;
	currentArc->flow = currentArc->capacity;
	parent->outOfTree[parent->numOutOfTree] = currentArc;
	++ parent->numOutOfTree;
	breakRelationship (parent, child);

	addToStrongBucket (child, strongRoots[child->label].end);
}


static inline void
//pushDownward (Arc *currentArc, Node *child, Node *parent, int flow)
pushDownward (Arc *currentArc, Node *child, Node *parent, long long flow)
{
#ifdef STATS
	++ numPushes;
#endif

	if (flow >= child->excess) 
	{
		parent->excess += child->excess;
		currentArc->flow -= child->excess;
		child->excess = 0;
		return;
	}

	currentArc->direction = 1;
	child->excess -= flow;
	parent->excess += flow;
	currentArc->flow = 0;
	parent->outOfTree[parent->numOutOfTree] = currentArc;
	++ parent->numOutOfTree;
	breakRelationship (parent, child);

	addToStrongBucket (child, strongRoots[child->label].end);
}

static void
pushExcess (Node *strongRoot) 
{
	Node *current, *parent;
	Arc *arcToParent;

	for (current = strongRoot; (current->excess && current->parent); current = parent) 
	{
		parent = current->parent;
		arcToParent = current->arcToParent;
		if (arcToParent->direction)
		{
			pushUpward (arcToParent, current, parent, (arcToParent->capacity - arcToParent->flow)); 
		}
		else
		{
			pushDownward (arcToParent, current, parent, arcToParent->flow); 
		}
	}

	if (current->excess > 0) 
	{
		if (!current->next)
		{
			addToStrongBucket (current, strongRoots[current->label].end);
		}
	}
}


static Arc *
findWeakNode (Node *strongNode, Node **weakNode) 
{
	int i, size;
	Arc *out;

	size = strongNode->numOutOfTree;

	for (i=strongNode->nextArc; i<size; ++i) 
	{

#ifdef STATS
		++ numArcScans;
#endif

		if (adjacencyList[strongNode->outOfTree[i]->to].label == (highestStrongLabel-1))
		{
			strongNode->nextArc = i;
			out = strongNode->outOfTree[i];
			(*weakNode) = &adjacencyList[out->to];
			-- strongNode->numOutOfTree;
			strongNode->outOfTree[i] = strongNode->outOfTree[strongNode->numOutOfTree];
			return (out);
		}
		else if (adjacencyList[strongNode->outOfTree[i]->from].label == (highestStrongLabel-1))
		{
			strongNode->nextArc = i;
			out = strongNode->outOfTree[i];
			(*weakNode) = &adjacencyList[out->from];
			-- strongNode->numOutOfTree;
			strongNode->outOfTree[i] = strongNode->outOfTree[strongNode->numOutOfTree];
			return (out);
		}
	}

	strongNode->nextArc = strongNode->numOutOfTree;

	return NULL;
}


static void
checkChildren (Node *curNode) 
{
	for ( ; (curNode->nextScan); curNode->nextScan = curNode->nextScan->next)
	{
		if (curNode->nextScan->label == curNode->label)
		{
			return;
		}
		
	}	

	-- labelCount[curNode->label];
	++	curNode->label;
	++ labelCount[curNode->label];

#ifdef STATS
	++ numRelabels;
#endif

	curNode->nextArc = 0;
}

static void
processRoot (Node *strongRoot) 
{
	Node *temp, *strongNode = strongRoot, *weakNode;
	Arc *out;

	strongRoot->nextScan = strongRoot->childList;

	if ((out = findWeakNode (strongRoot, &weakNode)))
	{
		merge (weakNode, strongNode, out);
		pushExcess (strongRoot);
		return;
	}

	checkChildren (strongRoot);
	
	while (strongNode)
	{
		while (strongNode->nextScan) 
		{
			temp = strongNode->nextScan;
			strongNode->nextScan = strongNode->nextScan->next;
			strongNode = temp;
			strongNode->nextScan = strongNode->childList;

			if ((out = findWeakNode (strongNode, &weakNode)))
			{
				merge (weakNode, strongNode, out);
				pushExcess (strongRoot);
				return;
			}

			checkChildren (strongNode);
		}

		if ((strongNode = strongNode->parent))
		{
			checkChildren (strongNode);
		}
	}

	addToStrongBucket (strongRoot, strongRoots[strongRoot->label].end);

	++ highestStrongLabel;
}

static Node *
getHighestStrongRoot (const int theparam) 
{
	int i;
	Node *strongRoot;

	for (i=highestStrongLabel; i>0; --i) 
	{
		if (strongRoots[i].start->next != strongRoots[i].end)  
		{
			highestStrongLabel = i;
			if (labelCount[i-1]) 
			{
				strongRoot = strongRoots[i].start->next;
				strongRoot->next->prev = strongRoot->prev;
				strongRoot->prev->next = strongRoot->next;
				strongRoot->next = NULL;
				return strongRoot;				
			}

			while (strongRoots[i].start->next != strongRoots[i].end) 
			{

#ifdef STATS
				++ numGaps;
#endif
				strongRoot = strongRoots[i].start->next;
				strongRoot->next->prev = strongRoot->prev;
				strongRoot->prev->next = strongRoot->next;
				liftAll (strongRoot, theparam);
			}
		}
	}

	if (strongRoots[0].start->next == strongRoots[0].end) 
	{
		return NULL;
	}

	while (strongRoots[0].start->next != strongRoots[0].end) 
	{
		strongRoot = strongRoots[0].start->next;
		strongRoot->next->prev = strongRoot->prev;
		strongRoot->prev->next = strongRoot->next;

		strongRoot->label = 1;
		-- labelCount[0];
		++ labelCount[1];

#ifdef STATS
		++ numRelabels;
#endif

		addToStrongBucket (strongRoot, strongRoots[strongRoot->label].end);
	}	

	highestStrongLabel = 1;

	strongRoot = strongRoots[1].start->next;
	strongRoot->next->prev = strongRoot->prev;
	strongRoot->prev->next = strongRoot->next;
	strongRoot->next = NULL;

	return strongRoot;	
}

static void
updateCapacities (const int theparam)
{
	int i, size;
	int delta;
	Arc *tempArc;
	Node *tempNode;

	long long param = curr_param;//lambdaStart + ((lambdaEnd-lambdaStart) *  ((float)theparam/(numParams-1)));
	size = adjacencyList[source].numOutOfTree;
	for (i=0; i<size; ++i)
	{
		tempArc = adjacencyList[source].outOfTree[i];
		//delta = (tempArc->capacities[theparam] - tempArc->capacity);
		delta = ( computeArcCapacity(tempArc, param) - tempArc->capacity);
		if (delta < 0)
		{
      printf ("c Error on source-adjacent arc (%d, %d): capacity decreases by %lf (%lf minus %lf) at parameter %d.\n",
          tempArc->from ,
          tempArc->to ,
          (double)(-delta)/APP_VAL,
          (double)computeArcCapacity(tempArc, param)/APP_VAL, //tempArc->capacities[theparam],
          (double)tempArc->capacity/APP_VAL,
          (theparam+1));
			exit(0);
		}

		tempArc->capacity += delta;
		tempArc->flow += delta;
		adjacencyList[tempArc->to].excess += delta;

		if ((adjacencyList[tempArc->to].label < numNodes) && (adjacencyList[tempArc->to].excess > 0))
		{
			pushExcess (&adjacencyList[tempArc->to]);
		}
	}

	size = adjacencyList[sink].numOutOfTree;
	for (i=0; i<size; ++i)
	{
		tempArc = adjacencyList[sink].outOfTree[i];
		//delta = (tempArc->capacities[theparam] - tempArc->capacity);
		delta = (computeArcCapacity(tempArc, param) - tempArc->capacity);
		if (delta > 0)
		{
			/*
			printf ("c Error on sink-adjacent arc (%d, %d): capacity %d increases to %d at parameter %d.\n",
				tempArc->from ,
				tempArc->to ,
				tempArc->capacity,
				tempArc->capacities[theparam],
				(theparam+1));
				*/
			exit(0);
		}

		tempArc->capacity += delta;
		tempArc->flow += delta;
		adjacencyList[tempArc->from].excess -= delta;

		if ((adjacencyList[tempArc->from].label < numNodes) && (adjacencyList[tempArc->from].excess > 0))
		{
			pushExcess (&adjacencyList[tempArc->from]);
		}
	}

	highestStrongLabel = (numNodes-1);
}

static long long 
computeMinCut (void)
{
	int i;
	long long mincut=0;

	for (i=0; i<numArcs; ++i) 
	{
		if ((adjacencyList[arcList[i].from].label >= numNodes) && (adjacencyList[arcList[i].to].label < numNodes))
		{
			mincut += arcList[i].capacity;
		}
	}
	return mincut;
}

static void
pseudoflowPhase1 (void) 
{
	Node *strongRoot;
	int theparam = 0;
	double thetime;

	thetime = timer ();
	while ((strongRoot = getHighestStrongRoot (theparam)))  
	{ 
		processRoot (strongRoot);
	}
	printf ("c Finished solving parameter %d\nc Flow: %lf\nc Elapsed time: %.3f\n", 
		(theparam+1),
		(double)computeMinCut ()/APP_VAL,
		(timer () - thetime));


	//for (; curr_param <= end_param; curr_param += step_param)
	//{
	
	while (isThereNextParam(theparam+1))
	{
		++theparam;
		curr_param = getParameter(theparam);
		updateCapacities (theparam);
#ifdef PROGRESS
		printf ("c Finished updating capacities and excesses.\n");
		fflush (stdout);
#endif
		while ((strongRoot = getHighestStrongRoot (theparam)))  
		{ 
			processRoot (strongRoot);
		}
		printf ("c Finished parameter: %d\nc Flow: %lf\nc Elapsed time: %.3f\n", 
			(theparam+1),
			(double)computeMinCut ()/APP_VAL,
			(timer () - thetime));
	}
}

static void
checkOptimality (void) 
{
	int i, check = 1;
	//llint mincut = 0, *excess;
    long long mincut=0, *excess;

	excess = (llint *) malloc (numNodes * sizeof (llint));
	if (!excess)
	{
		printf ("%s Line %d: Out of memory\n", __FILE__, __LINE__);
		exit (1);
	}

	for (i=0; i<numNodes; ++i)
	{
		excess[i] = 0;
	}

	for (i=0; i<numArcs; ++i) 
	{
		if ((adjacencyList[arcList[i].from].label >= numNodes) && (adjacencyList[arcList[i].to].label < numNodes))
		{
			mincut += arcList[i].capacity;
		}

		if ((arcList[i].flow > arcList[i].capacity) || (arcList[i].flow < 0)) 
		{
			check = 0;
			printf("c Capacity constraint violated on arc (%d, %d)\n",
				arcList[i].from ,
				arcList[i].to );
		}
		excess[arcList[i].from] -= arcList[i].flow;
		excess[arcList[i].to] += arcList[i].flow;
	}

	for (i=0; i<numNodes; i++) 
	{
		if ((i != (source)) && (i != (sink)))
		{
			if (excess[i]) 
			{
				check = 0;
				/*printf ("c Flow balance constraint violated in node %d. Excess = %lld\n",
					i+1,
					excess[i]);*/
                printf("c Flow balance constraint violated in node %d, Excess = %lf\n", i+1, excess[i]);
			}
		}
	}

	if (check)
	{
		printf ("c\nc Solution checks as feasible.\n");
	}

	check = 1;

	if (excess[sink] != mincut)
	{
		check = 0;
		printf("c Flow is not optimal - max flow does not equal min cut!\nc\n");
	}

	if (check)
    {
        printf ("c\nc Solution checks as optimal.\nc \n");
        //printf ("s Max Flow            : %lld\n", mincut);
        printf ("s Max Flow            : %lf\n", mincut);
    }
	free (excess);
	excess = NULL;
}


static void
quickSort (Arc **arr, const int first, const int last)
{
	int i, j, left=first, right=last, x1, x2, x3, mid, pivot, pivotval;
	Arc *swap;

	if ((right-left) <= 5)
	{// Bubble sort if 5 elements or less
		for (i=right; (i>left); --i)
		{
			swap = NULL;
			for (j=left; j<i; ++j)
			{
				if (arr[j]->flow < arr[j+1]->flow)
				{
					swap = arr[j];
					arr[j] = arr[j+1];
					arr[j+1] = swap;
				}
			}

			if (!swap)
			{
				return;
			}
		}

		return;
	}

	mid = (first+last)/2;

	x1 = arr[first]->flow; 
	x2 = arr[mid]->flow; 
	x3 = arr[last]->flow;

	pivot = mid;
	
	if (x1 <= x2)
	{
		if (x2 > x3)
		{
			pivot = left;

			if (x1 <= x3)
			{
				pivot = right;
			}
		}
	}
	else
	{
		if (x2 <= x3)
		{
			pivot = right;

			if (x1 <= x3)
			{
				pivot = left;
			}
		}
	}

	pivotval = arr[pivot]->flow;

	swap = arr[first];
	arr[first] = arr[pivot];
	arr[pivot] = swap;

	left = (first+1);

	while (left < right)
	{
		if (arr[left]->flow < pivotval)
		{
			swap = arr[left];
			arr[left] = arr[right];
			arr[right] = swap;
			-- right;
		}
		else
		{
			++ left;
		}
	}

	swap = arr[first];
	arr[first] = arr[left];
	arr[left] = swap;

	if (first < (left-1))
	{
		quickSort (arr, first, (left-1));
	}
	
	if ((left+1) < last)
	{
		quickSort (arr, (left+1), last);
	}
}

static void
sort (Node * current)
{
	if (current->numOutOfTree > 1)
	{
		quickSort (current->outOfTree, 0, (current->numOutOfTree-1));
	}
}

static void
minisort (Node *current) 
{
	Arc *temp = current->outOfTree[current->nextArc];
	int i, size = current->numOutOfTree, tempflow = temp->flow;

	for(i=current->nextArc+1; ((i<size) && (tempflow < current->outOfTree[i]->flow)); ++i)
	{
		current->outOfTree[i-1] = current->outOfTree[i];
	}
	current->outOfTree[i-1] = temp;
}

static void
decompose (Node *excessNode, const int source, int *iteration) 
{
	Node *current = excessNode;
	Arc *tempArc;
    //int bottleneck = excessNode->excess;
    long long bottleneck = excessNode->excess;

	for ( ;(current - adjacencyList+1 != source) && (current->visited < (*iteration));
				current = &adjacencyList[tempArc->from])
	{
		current->visited = (*iteration);
		tempArc = current->outOfTree[current->nextArc];

		if (tempArc->flow < bottleneck)
		{
			bottleneck = tempArc->flow;
		}
	}

	if (current - adjacencyList+1 == source)
	{
		excessNode->excess -= bottleneck;
		current = excessNode;

		while (current - adjacencyList+1 != source)
		{
			tempArc = current->outOfTree[current->nextArc];
			tempArc->flow -= bottleneck;

			if (tempArc->flow)
			{
				minisort(current);
			}
			else
			{
				++ current->nextArc;
			}
			current = &adjacencyList[tempArc->from];
		}
		return;
	}

	++ (*iteration);

	bottleneck = current->outOfTree[current->nextArc]->flow;

	while (current->visited < (*iteration))
	{
		current->visited = (*iteration);
		tempArc = current->outOfTree[current->nextArc];

		if (tempArc->flow < bottleneck)
		{
			bottleneck = tempArc->flow;
		}
		current = &adjacencyList[tempArc->from];
	}

	++ (*iteration);

	while (current->visited < (*iteration))
	{
		current->visited = (*iteration);

		tempArc = current->outOfTree[current->nextArc];
		tempArc->flow -= bottleneck;

		if (tempArc->flow)
		{
			minisort(current);
			current = &adjacencyList[tempArc->from];
		}
		else
		{
			++ current->nextArc;
			current = &adjacencyList[tempArc->from];
		}
	}
}

static void
recoverFlow (void)
{
	int i, j, iteration = 1;
	Arc *tempArc;
	Node *tempNode;

	for (i=0; i<adjacencyList[sink].numOutOfTree; ++i)
	{
		tempArc = adjacencyList[sink].outOfTree[i];
		if (adjacencyList[tempArc->from].excess < 0)
		{
			tempArc->flow -= (long long) (-1*adjacencyList[tempArc->from].excess);
			adjacencyList[tempArc->from].excess = 0;
		}
	}

	for (i=0; i<adjacencyList[source].numOutOfTree; ++i)
	{
		tempArc = adjacencyList[source].outOfTree[i];
		addOutOfTreeNode (&adjacencyList[tempArc->to], tempArc);
	}

	adjacencyList[source].excess = 0;
	adjacencyList[sink].excess = 0;

	for (i=0; i<numNodes; ++i)
	{
		tempNode = &adjacencyList[i];

		if ((i == (source)) || (i == (sink)))
		{
			continue;
		}

		if (tempNode->label >= numNodes) 
		{
			tempNode->nextArc = 0;
			if ((tempNode->parent) && (tempNode->arcToParent->flow))
			{
				addOutOfTreeNode (&adjacencyList[tempNode->arcToParent->to], tempNode->arcToParent);
			}

			for (j=0; j<tempNode->numOutOfTree; ++j) 
			{
				if (!tempNode->outOfTree[j]->flow) 
				{
					-- tempNode->numOutOfTree;
					tempNode->outOfTree[j] = tempNode->outOfTree[tempNode->numOutOfTree];
					-- j;
				}
			}

			sort(tempNode);
		}
	}

	for (i=0; i<numNodes; ++i) 
	{
		tempNode = &adjacencyList[i];
		while (tempNode->excess > 0) 
		{
			++ iteration;
			decompose(tempNode, source, &iteration);
		}
	}
}


static void
displayBreakpoints (void)
{
	int i;
	for (i=0; i<numNodes; ++i)
	{
		printf ("n %d %d\n", (i+1), adjacencyList[i].breakpoint);
	}
}

static void
freeMemory (void)
{
	int i;

	for (i=0; i<numNodes; ++i)
	{
		freeRoot (&strongRoots[i]);
	}

	free (strongRoots);

	for (i=0; i<numNodes; ++i)
	{
		if (adjacencyList[i].outOfTree)
		{
			free (adjacencyList[i].outOfTree);
		}
	}

	if ( _params != NULL )
	{
		free (_params);
	}

	free (adjacencyList);

	free (labelCount);

	free (arcList);
}

int 
main(int argc, char ** argv) 
{

	printf ("c Pseudoflow algorithm for parametric min cut (version 1.0)\n");
	readDimacsFileCreateList ();

#ifdef PROGRESS
	printf ("c Finished reading file.\n"); fflush (stdout);
#endif

	simpleInitialization ();

#ifdef PROGRESS
	printf ("c Finished initialization.\n"); fflush (stdout);
#endif

	pseudoflowPhase1 ();

#ifdef PROGRESS
	printf ("c Finished phase 1.\n"); fflush (stdout);
#endif

#ifdef RECOVER_FLOW
	recoverFlow();
	checkOptimality ();
#endif

	printf ("c Number of nodes     : %d\n", numNodes);
	printf ("c Number of arcs      : %d\n", numArcs);
#ifdef STATS
	printf ("c Number of arc scans : %lld\n", numArcScans);
	printf ("c Number of mergers   : %d\n", numMergers);
	printf ("c Number of pushes    : %lld\n", numPushes);
	printf ("c Number of relabels  : %d\n", numRelabels);
	printf ("c Number of gaps      : %d\n", numGaps);
#endif

#ifdef BREAKPOINTS
	displayBreakpoints ();
#endif

	freeMemory ();

	return 0;
}
