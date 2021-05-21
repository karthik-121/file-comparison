#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>


struct queue* makeQueue();
int enqueue(struct queue* q, char* name);
char* dequeue(struct queue* q);
int is_Directory(const char *pathname);
void removeChar(char *str);
int checkSpace(char* str);
struct wfd* readfile( char* filename);
int compare (const void * a, const void * b);
int activeThreads = 0;
int factiveThreads = 0;
pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;


struct node{
	struct node* next;
	char* name;
	int count;
	int visited;
};


struct queue{
	struct node* front;
	struct node* back;
	int size;
	int open;
	int MAXSIZE;
	pthread_mutex_t lock;
	pthread_cond_t read_ready;
	pthread_cond_t write_ready;
};




struct node* allocate(char* str){
	struct node* temp = malloc(sizeof(struct node));
	temp->name = malloc(strlen(str)+1);
	strcpy(temp->name, str);
	temp->next = 0;
	temp->count = 1;
	temp->visited = 0;
	return temp; 
}


struct queue* makeQueue(){

	struct queue* q = malloc(sizeof(struct queue));
	q->front = 0; 
	q->back = 0;
	q->size = 0;
	q->MAXSIZE = 128;
	q->open = 1;
	pthread_mutex_init(&q->lock, NULL);
	pthread_cond_init(&q->read_ready, NULL);
	pthread_cond_init(&q->write_ready, NULL);
	return q;
}

int destroy ( struct queue* q){

	pthread_mutex_destroy(&q->lock);
	pthread_cond_destroy(&q->read_ready);
	pthread_cond_destroy(&q->write_ready);
	
	return 0;
}



int enqueue(struct queue* q, char* name){
	
	pthread_mutex_lock(&q->lock);
	while ( q->size == q->MAXSIZE && q->open){
		pthread_cond_wait(&q->write_ready, &q->lock);

	}
	if (!q->open){
		pthread_mutex_unlock(&q->lock);
		return -1;
	}
	struct node* newNode = allocate(name);
	struct node* ptr = q->front;
	struct node* last = 0;
	q->size++;
	

	if ( q->back == 0){
		q->front = newNode;
		q->back = newNode;
		pthread_cond_signal(&q->read_ready);
		pthread_mutex_unlock(&q->lock);
		return 0;
	}
	while( ptr!= 0 ){
		last = ptr;
		ptr = ptr->next;

	}
	last->next = newNode;
	q->back = newNode;
	pthread_cond_signal(&q->read_ready);
	pthread_mutex_unlock(&q->lock);
	return 0;
}

char* dequeue(struct queue* q){
	
	
	pthread_mutex_lock(&q->lock);
	
	while (q->size == 0 && q->open){
		pthread_cond_wait(&q->read_ready, &q->lock);
		 

	}

	if ( q->front == 0 ){
		activeThreads--;
		factiveThreads--;
		if ( activeThreads == 0) {
			pthread_mutex_unlock(&q->lock);
			pthread_cond_broadcast(&q->read_ready);
			return NULL;

		}
		while ( q->front == 0 && activeThreads != 0){
			pthread_cond_wait(&q->read_ready, &q->lock);

		}
		if ( q->front == 0){

			pthread_mutex_unlock(&q->lock);
			return NULL;

		}

		activeThreads++;
		factiveThreads++;
	} 
	
	char* name = q->front->name;
	struct node* temp = q->front;
	
	q->front = q->front->next; 
	free(temp);
	if ( q->front == 0){
		q->back = 0; 
	}
	q->size--;

	pthread_cond_signal(&q->write_ready);
	pthread_mutex_unlock(&q->lock);

	return name;
	

}

int qclose(struct queue* q){
	pthread_mutex_lock(&q->lock);
	q->open = 0;
	pthread_cond_broadcast(&q->read_ready);
	pthread_cond_broadcast(&q->write_ready);
	pthread_mutex_unlock(&q->lock);

	return 0;

}


struct queue* fileQueue;
struct queue* directoryQueue;

//FOR 1 FILE
struct wfd {

	struct node* head;
	int totalWords;
	char* filename;
	
	
};

struct repo_node{
	struct repo_node* next;
	struct wfd* list;
};

struct repo_node* RN_allocate(char* str){
	struct repo_node* temp = malloc(sizeof(struct node));
	temp->list = readfile(str);
	temp->next = 0;
	return temp; 
}



//FOR REPOSITORY
struct wfd_repo{

	struct repo_node* head;
	int size;
	

};

struct analysis{
	char* file1;
	char* file2;
	int combinedWords;
	double JSD;

};


//FOR 1 FILE
struct wfd* wfdinit(char* str) {
	struct wfd* list = malloc(sizeof(struct wfd));
	list->head = 0;
	list->totalWords = 0;
	list->filename = malloc(strlen(str) + 1);
	strcpy(list->filename, str);
	return list;

}

//FOR REPOSITORY
struct wfd_repo* createRepo(){

	struct wfd_repo* temp = malloc(sizeof(struct wfd_repo));
	temp->head = 0;
	temp->size = 0;
	return temp;


}

//FOR 1 FILE
void addWord (struct wfd* list, char* name) {

	struct node* newNode = allocate(name);
	
	
	if ( list->head == 0 ){
		list->head = newNode;
		list->totalWords++;
		
		return;

	}
	list->totalWords++;
	struct node* ptr = list->head;
	struct node* ptr2 = list->head;
	while ( ptr2 != 0) {
		if (strcmp(newNode->name, ptr2->name) == 0){
			ptr2->count++;
			free(newNode->name);
			free(newNode);
			return;

		}
		ptr2 = ptr2->next;

	}
	struct node* prev = 0;
	
	while ( ptr != 0) {
		if ( strcmp(newNode->name, ptr->name) < 0) {
			break;
		}
		prev = ptr;
		ptr = ptr->next;

	}
	if ( ptr == 0) { prev->next = newNode;}
	else {
		if ( prev != 0) {
			newNode->next = prev->next;
			prev->next = newNode;

		}
		else{
			newNode->next = list->head;
			list->head = newNode;
			

		}

	}
	return;
}


//FOR REPOSITORY
void addList( struct wfd_repo* repo, char* str){
	struct repo_node* newNode = RN_allocate(str);
	repo->size++;
	
	if ( repo->head == 0) {

		repo->head = newNode;
		return;

	}
	struct repo_node* ptr = repo->head;
	struct repo_node* last = 0;
	while (ptr!= 0){
		last = ptr;
		ptr = ptr->next;

	}
	last->next = newNode;
	

}

//FOR 1 FILE
struct wfd* readfile( char* filename) {
	int file = open(filename, O_RDONLY);
	struct wfd* list = wfdinit(filename);
	char* c = malloc(sizeof(char) * INT_MAX);
	int sz;
	int i = 0;
	do {
		
		char character = 0;
		sz = read(file, &character, 1);
		c[i++] = character;

	} while ( sz != 0);
	i--;
	c[i] = '\0';
	
	while(isspace(c[0]) || ispunct(c[0]) != 0){ // check all starting spaces till there is a letter

		for(int k=0;k<i;k++){
			c[k] = c[k+1];
		}
		i--;
		
	}
	if ( i == 0) {
		free(c);
		return list;

	}
	c[i] = '\0';
	int k = 0;
	char* wordArray;
	while ( k < i) {
		int prevRef = k;
		while ( k <= i ){

			if ( c[k] == 39 || c[k] == '-'){
				k++;
				continue;
			}
	
			if ( (isspace(c[k]) != 0 || ispunct(c[k]) != 0 )  ){
				k++;
				break;
			}
			else{
				k++;
			}
		}
		int u = k;

		
		int wordLength = (k - prevRef) - 1;
		wordArray = (char *) calloc(wordLength + 1, sizeof(char));
		int arrayCounter = 0;
		for ( int j = prevRef; j < k - 1; j++){
			wordArray[arrayCounter] = toupper(c[j]);
			arrayCounter++; 
		}
		removeChar(wordArray);
		
		addWord(list, wordArray);
		
		while ( isspace(c[u]) != 0 || ispunct(c[u]) != 0 ){
			
			
			u++; 
		}
		
		
		k = u;
		free(wordArray);
		

	}
	free(c);
	close(file);
	
	return list;
	
	
	
}

void* addAllFiles (void* A){
	
	pthread_mutex_lock(&lock1);
	
	if ( fileQueue->size <= 0 && directoryQueue->size <= 0){
		
		pthread_mutex_unlock(&lock1);
		pthread_exit(NULL);
		return NULL;
	}
	
	
	
	char* pathname = dequeue(fileQueue);
	struct wfd_repo* regis = (struct wfd_repo* ) A;
	addList(regis, pathname);
	free(pathname);

	pthread_mutex_unlock(&lock1);
	
	if ( directoryQueue->size != 0 || fileQueue->size != 0) {
		
		addAllFiles(A);
	}
	return NULL;


}


void *traverseDirectories(void* A){
	
	pthread_mutex_lock(&lock1);
	if ( directoryQueue->size <= 0){
		
		pthread_mutex_unlock(&lock1);
		return NULL;
		
		
		
	}
	
	
	char* pathname = dequeue(directoryQueue);
	char* suffix = (char*) A;
	struct dirent *sd;
	DIR *dir;
	dir = opendir(pathname);
	struct stat filestat;
	if (dir == NULL){
		fprintf(stderr, "cant open %s", pathname);
		perror("error message printed");
	} 
	else{

		while (     (sd = readdir(dir)) != NULL    ){

			//skip . and .. and any other entries that start with .
			if ( sd->d_name[0] != '.'){
				
				
				//concatinating pathname with entry name
				char* wordarray = malloc(sizeof(char) * (strlen(pathname) + strlen(sd->d_name) + 2));
				strcpy(wordarray, pathname); 
				strcat(wordarray, "/");
				strcat(wordarray, sd->d_name);

				stat(wordarray, &filestat);
				if (S_ISDIR(filestat.st_mode)) {

					enqueue(directoryQueue, wordarray);
			
				
		
				}
				//for regular files
				else {
					int y = strlen(sd->d_name) - strlen(suffix);
					if ( y < 0){
						continue;
					}
					//checking if suffix equals file suffix
					int i;
					int j = 0; 
					int check_word = 0;
					for ( i = y; i < strlen(sd->d_name); i++){
						if ( sd->d_name[i] != suffix[j] ){
							check_word = 1;
							break;
						}
						j++;
					}
					//suffixes match, add file to file queue
					if (check_word != 1){
						
						int file;
						file = open(wordarray, O_RDONLY);
						if ( file < 0){
							fprintf(stderr, "cant open %s\n", wordarray);
							perror("error message printed");
							continue;
						}
						else {
							enqueue(fileQueue, wordarray);
							close(file);
						}
				

						

					}
				}

				free(wordarray);

			}
			else {
				continue;
			}
		}

	}
	free(pathname);
	closedir(dir);
	pthread_mutex_unlock(&lock1);
	if(directoryQueue->size != 0){
		traverseDirectories(A);
	} 
	
	return NULL;
	

}

double calcJSD(struct repo_node* ptr1, struct repo_node* ptr2){
	
	struct node* head1 = ptr1->list->head;
	double kld1 = 0;
	double kld2 = 0;
	int file1check = 0;
	double total1 = ptr1->list->totalWords;
	double total2 = ptr2->list->totalWords;

	while (head1 != 0){
		struct node* head2 = ptr2->list->head;
		
				
		
		while ( head2 != 0){
			if(strcmp(head1->name, head2->name) == 0){
				
				double x = head1->count/total1; //find frequency of word in file1
				
				double y = head2->count/total2; //find frequency of word in file2
				
				double z = (x + y)/2;
				kld1 += (x * (log2(x/z)));
				kld2 += (y * (log2(y/z)));
				file1check = 1; 
				head2->visited = 1; //print to check correctness, keeping mark of all words visited in file 2, as some may be skipped
				break;
				
				

			}
			head2 = head2->next;
			
		}
		//for word that is not in file2
		if (file1check == 0){
			
			double x = head1->count/total1;
			double z = x/2;
			kld1 += x * (log2(x/z));
			

		}
		file1check = 0;
		head1 = head1->next;
	}
	//for file2 words that were not visited b/c no matches
	struct node* head3 = ptr2->list->head;
	while ( head3 != 0){
		if ( head3->visited != 1){
			double y = head3->count/total2;
			double z = y/2;
			kld2 += y * (log2(y/z));
			

		}
		head3->visited = 0;
		head3 = head3->next;

	}
	double JSDsquared = (0.5*kld1) + (0.5*kld2);
	double KLDsqrt = sqrt(JSDsquared);
	return KLDsqrt;

}

struct t_args{

	struct analysis* JSD_calc;
	struct wfd_repo* repo_list;



};

struct d_args{

	struct queue *q;
	int id;
	int max;
	int wait;


};

void* allCalculations(void* A){
	pthread_mutex_lock(&lock1);
	struct t_args* t = (struct t_args*) A;
	 
	struct repo_node* ptr = t->repo_list->head; //ptr to first file
	int i = 0;
	while (ptr != 0){
		struct repo_node* ptr2 = ptr->next; //ptr to second file
		while ( ptr2 != 0){
			//retrieve information from both files
			t->JSD_calc[i].file1 = ptr->list->filename;
			t->JSD_calc[i].file2 = ptr2->list->filename;
			t->JSD_calc[i].combinedWords = ptr->list->totalWords + ptr2->list->totalWords;
			t->JSD_calc[i].JSD = calcJSD(ptr, ptr2);
			
			ptr2 = ptr2->next;
			i++;

		}
		ptr = ptr->next;


	}
	pthread_mutex_unlock(&lock1);
	return NULL;


}

void freeWFDNodes( struct wfd* list){
	struct node* ptr = list->head;
	while (list->head != 0){
		ptr = list->head;
		list->head = list->head->next;
		free(ptr->name);
		free(ptr);

	}
	free(list->filename);
	free(list);


}

void freeWFD_repo(struct wfd_repo* list){
	
	struct repo_node* ptr;
	while ( list->size != 0){
		ptr = list->head;
		list->head = list->head->next;
		freeWFDNodes(ptr->list);
		free(ptr);
		list->size--;


	}


}

int main(int argc, char *argv[])
{
	//checking if argument begins with "-"
	int fileThreads = 1;
	int directoryThreads = 1;
	int analysisThreads = 1;
	char* suffix = ".txt";
	fileQueue = makeQueue();
	directoryQueue = makeQueue();
	
	pthread_t *ttid;
	pthread_t *atid;
	for ( int i = 1; i < argc; i++){

		
		if (is_Directory(argv[i]) != 0) {
				

				enqueue(directoryQueue, argv[i]);
				
				continue;

		
		}
		if ( (argv[i][0]) == '-'){


			
			
			//adding digits to char[] to get number of threads
			char* number = &(argv[i][2]);
				
				
			if ( (argv[i][1]) == 'd'){
				//invalid if argv is less than 3
				if ( strlen(argv[i]) < 3 || argv[i][2] == '0') {

					perror("Invalid options argument");
					return EXIT_FAILURE;

				}
				//checking for random letters in argument
				for ( int j = 2; j < strlen(argv[i]); j++){
					if ( isdigit(argv[i][j]) == 0 ){
						perror("Invalid options argument");
						return EXIT_FAILURE;
					}
				}
					
				directoryThreads = atoi(number);
				


			}
			if ( (argv[i][1]) == 'f' ){
				if ( strlen(argv[i]) < 3 || argv[i][2] == '0') {

					perror("Invalid options argument");
					return EXIT_FAILURE;

				}
				for ( int j = 2; j < strlen(argv[i]); j++){
					if ( isdigit(argv[i][j]) == 0 ){
						perror("Invalid options argument");
						return EXIT_FAILURE;
					}
				}
				fileThreads = atoi(number);
				
					
			}
			if ( (argv[i][1]) == 'a' ){
				if ( strlen(argv[i]) < 3 || argv[i][2] == '0') {

					perror("Invalid options argument");
					return EXIT_FAILURE;

				}
				for ( int j = 2; j < strlen(argv[i]); j++){
					if ( isdigit(argv[i][j]) == 0 ){
						perror("Invalid options argument");
						return EXIT_FAILURE;
					}
				}
				analysisThreads = atoi(number);
				
			}
			if ( (argv[i][1]) == 's'){

				suffix = &(argv[i][2]);
				
			}
			
		} else {
			int file;
			file = open(argv[i], O_RDONLY);
			if ( file < 0){
				fprintf(stderr, "%s is not a file\n", argv[i]);
				perror("error message printed");
				continue;
			}
			else {
				
				enqueue(fileQueue, argv[i]);
				close(file);
			}
		}
		
	}
	if ( fileQueue->size == 0 && directoryQueue->size == 0){
		printf("Not enough files found\n");
		return EXIT_FAILURE;

	}
	
	
	int totalThreads = directoryThreads + fileThreads;
	ttid = malloc(totalThreads * sizeof(pthread_t));
	
	atid = malloc(analysisThreads * sizeof(pthread_t));
	struct wfd_repo* repo_list = createRepo();
	
	
	int k;
	for ( k = 0; k < directoryThreads; k++){
		activeThreads++;
		pthread_create(&ttid[k], NULL, traverseDirectories, suffix);
		
		
		

	}

	for ( ; k < totalThreads;k++ ){
		factiveThreads++;
		pthread_create(&ttid[k], NULL, addAllFiles, repo_list);
		
		
	} 
	
	sleep(3);
	qclose(fileQueue);
	qclose(directoryQueue);

	
	for ( k = 0; k < totalThreads; k++){
		pthread_join(ttid[k], NULL);

	}
	destroy(fileQueue);
	destroy(directoryQueue);
	if ( repo_list->size < 2){
		printf("Not enough files found\n");
		free(ttid);
		free(atid);
		freeWFD_repo(repo_list);
		free(repo_list);
		return EXIT_FAILURE;
	}
	
	
	int combinations = (0.5 * repo_list->size)*(repo_list->size - 1);
	
	struct t_args* thread_args = malloc(sizeof(struct t_args));
	thread_args->JSD_calc = malloc(sizeof(struct analysis) * combinations);
	thread_args->repo_list = repo_list;
	for ( int i = 0; i < analysisThreads; i++){
		pthread_create(&atid[i], NULL, allCalculations, thread_args);
		
	
	}
	for ( int i = 0; i < analysisThreads; i++ ){
		pthread_join(atid[i], NULL);

	} 
	qsort(thread_args->JSD_calc, combinations, sizeof(struct analysis), compare );
	for ( int i = 0; i < combinations; i++){
		printf("%g %s %s\n", thread_args->JSD_calc[i].JSD, thread_args->JSD_calc[i].file1, thread_args->JSD_calc[i].file2 );
		

	}
	
	
	
	freeWFD_repo(repo_list);
	free(repo_list);
	free(thread_args->JSD_calc);
	free(thread_args);
	
	
	free(ttid);
	free(atid);

	


	return EXIT_SUCCESS;
}


int is_Directory(const char *pathname) {
   struct stat statbuf;
   if (stat(pathname, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

void removeChar(char *str) {
	
    	char *src, *dst;
	char a = 39;
    	for (src = dst = str; *src != '\0'; src++) {
       		*dst = *src;
       		if (*dst != a) dst++;
   	}
    	*dst = '\0';
}
int checkSpace(char* str) {
	
	for ( int i = 0; i < strlen(str) ; i++){

		if ( isspace(str[i]) || (ispunct(str[i]) && str[i] != '-')) {
			
			return 1;

		} 
	
	}
	return 0;

}

int compare (const void * a, const void * b)
{

  struct analysis *combinedA = (struct analysis* )a;
  struct analysis *combinedB = (struct analysis*)b;

  return  combinedB->combinedWords - combinedA->combinedWords;
}





