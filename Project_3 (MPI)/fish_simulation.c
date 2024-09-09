#include <mpi.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include "../../../usr/lib/x86_64-linux-gnu/openmpi/include/mpi.h"

struct speed{
    double x;
    double y;
    double z;
};

struct fish{
    double x;
    double y;
    double z;
    struct speed speed;
    char active;
    long id;
    int size;
    int eating;
};

struct parameters{
    int numb_fish;
    int edge_size;
    double speed;
    double eating_distance;
    int timestep;
    int report;
};

struct node{
    struct fish* fish;
    struct node* prev;
    struct node* next;
    struct node* head;
};

struct update_tuple{
    long id;
    int grow;
};

struct ctx{
    double start_y;
    double end_y;
    int adjacent_to_consider;
    int my_rank;
    int world_size;
    struct node* fishes;
    struct fish** adjacent_list;
    int adjacent_list_size;
    int* adjacent_sizes;
    struct update_tuple** update_lists;
    int* neighbor_ranks;
    int number_of_neighbors;

};


MPI_Datatype type_fish;
MPI_Datatype type_speed;
struct parameters params;
struct ctx ctx;

void print_fish(struct fish f,int l){
    if(l)
        printf("%d-LOCAL Fish:%ld       Position:%fx   %fy   %fz   SPEED:%fx   %fy   %fz   active:%d   size:%d    eating:%d\n",
                    ctx.my_rank,f.id,f.x,f.y,f.z,f.speed.x,f.speed.y,f.speed.z,f.active,f.size,f.eating);
    else
        printf("%d-NON LOCAL Fish:%ld       Position:%fx   %fy   %fz   SPEED:%fx   %fy   %fz   active:%d   size:%d    eating:%d\n",
                    ctx.my_rank,f.id,f.x,f.y,f.z,f.speed.x,f.speed.y,f.speed.z,f.active,f.size,f.eating);
}

void print_local(){
    struct node* c = ctx.fishes->next;
    while(c!=NULL){
        print_fish(*c->fish,1);
        c = c->next;
    }
}

void print_array(struct fish* f, int size){
    for(int i=0;i<size;i++)
        print_fish(f[i],0);
}

//calculate the distance between two fishes
double distance(struct fish* f1,struct fish* f2){
    double x,y,z,res;
    x = f1->x - f2->x;
    y = f1->y - f2->y;
    z = f1->z - f2->z;
    res = sqrt((x*x) + (y*y) + (z*z));
    return res;
}

//move the fish of the speed*timestep
void move_fish(struct fish* f){
    f->x += f->speed.x * params.timestep;
    f->y += f->speed.y * params.timestep;
    f->z += f->speed.z * params.timestep;

    //CHECKING BOUNDRIES
    //if a boundry is crossed then position set at the boundry 
    //of the dimention and the speed is reversed on that axis

    if(f->x >= params.edge_size){
        f->x = params.edge_size;
        f->speed.x = f->speed.x * -1;
    }
    if(f->x <= 0){
        f->x = 0;
        f->speed.x = f->speed.x * -1;
    }

    if(f->y >= params.edge_size){
        f->y = (float)params.edge_size-0.001;
        f->speed.y = f->speed.y * -1;
    }
    if(f->y <= 0){
        f->y = 0;
        f->speed.y = f->speed.y * -1;
    }

    if(f->z >= params.edge_size){
        f->z = params.edge_size;
        f->speed.z = f->speed.z * -1;
    }
    if(f->z <= 0){
        f->z = 0;
        f->speed.z = f->speed.z * -1;
    }
}

//sorted add in a list
struct node* add(struct fish f, struct node* list){
    struct node* to_add = malloc(sizeof(struct node));
    
    struct node* prev = list->head;
    struct node* current = list->head->next;
    while(current != NULL){
        if(f.size > current->fish->size){
            //if fish is bigger in size then the one in current node
            //put it in front of the current node
            to_add->fish = malloc(sizeof(struct fish));
            *(to_add->fish) = f;
            to_add->next = current;
            to_add->prev = prev;
            to_add->head = list->head;
            prev->next = to_add;
            current->prev = to_add;
            return list;       
        }
        prev = prev->next;
        current = current->next;
    }
    prev->next = to_add;
    to_add->fish = malloc(sizeof(struct fish));
    to_add->head = list->head;
    *(to_add->fish) = f;
    to_add->prev = prev;
    to_add->next = NULL;
    return list;
}

//remove_node a node in a list
void remove_node(struct node* node){
    node->prev->next = node->next;
    if(node->next!=NULL) 
       node->next->prev = node->prev;
    free(node->fish);
    free(node);
}

int count_local(){
    struct node* current = ctx.fishes->next;
    int res = 0;

    while(current!=NULL){
        res++;
        current = current->next;
    }
    return res;

}

struct fish* list_to_array(int* len){

    struct node* current = ctx.fishes->next;
    //very costly due to poor design, might change later
    int count_of_local_fishes = count_local();


    struct fish* res = malloc(sizeof(struct fish)*count_of_local_fishes);

    int i = 0;
    while(current != NULL){
        res[i] = *(current->fish);
        i++;
        current = current->next;
    }
    *len = count_of_local_fishes;
    return res;
}

void send_neighbor(){

    MPI_Request req[ctx.number_of_neighbors*2];
    struct fish* to_send;
    int len_to_send;

    to_send = list_to_array(&len_to_send);

    //we first send the len of the buffer then the buffer
    int j = 0;
    for(int i = 0;i<ctx.number_of_neighbors;i++){
            MPI_Isend(&len_to_send,1, MPI_INT, ctx.neighbor_ranks[i], 0, MPI_COMM_WORLD, &req[j]);
            j++;
            MPI_Isend(to_send,len_to_send, type_fish, ctx.neighbor_ranks[i], 0, MPI_COMM_WORLD, &req[j]);
            j++;
    }

    MPI_Status stats[j];
    MPI_Waitall(j,req,stats);
}

//send the info of the adjacent fishes to the owners
void send_adj(){
    MPI_Request req[ctx.number_of_neighbors*2];

    //we first send the len of the buffer then the buffer
    int j = 0;
    for(int i = 0;i<ctx.number_of_neighbors;i++){
            int rank = ctx.neighbor_ranks[i];
            MPI_Isend(&ctx.adjacent_sizes[rank],1, MPI_INT, rank , 0, MPI_COMM_WORLD, &req[j]);
            j++;
            MPI_Isend(ctx.adjacent_list[rank],ctx.adjacent_sizes[rank], type_fish, rank , 0, MPI_COMM_WORLD, &req[j]);
            j++;
    }
    MPI_Status stats[j];
    MPI_Waitall(j,req,stats);
}

//receive from neighbors
int recv_neighbor(){

    MPI_Request req[ctx.adjacent_to_consider];

    int j = 0;
    for(int i = 0;i<ctx.number_of_neighbors;i++){
        MPI_Irecv(&ctx.adjacent_sizes[ctx.neighbor_ranks[i]],1,MPI_INT,ctx.neighbor_ranks[i],0,MPI_COMM_WORLD,&req[j]);
        j++;
    }
    
    MPI_Status stats[j];
    MPI_Waitall(j,req,stats);

    //allocate the arrays where to receive
    for(int i = 0;i<ctx.number_of_neighbors;i++){
        int size = ctx.adjacent_sizes[ctx.neighbor_ranks[i]];
        ctx.adjacent_list[ctx.neighbor_ranks[i]] = malloc(sizeof(struct fish) * size);
    }
    
    j=0;
    MPI_Request req2[ctx.adjacent_to_consider];
    for(int i = 0;i<ctx.number_of_neighbors;i++){
        int rank = ctx.neighbor_ranks[i];
        MPI_Irecv(ctx.adjacent_list[rank],ctx.adjacent_sizes[rank],type_fish,rank,0,MPI_COMM_WORLD,&req2[j]);
        j++;
    }
    MPI_Status stats2[j];
    MPI_Waitall(j,req2,stats2);
}


//merges updates the local list based on the info received by the neighbors
//it increses the size if the fish ate or removes it if it has been eaten
void merge_neighbors(){

    struct node* cycle_local;

    for(int i = 0;i<ctx.number_of_neighbors;i++){
        int rank = ctx.neighbor_ranks[i]; 
        cycle_local = ctx.fishes->next;

        for (int j = 0; j<ctx.adjacent_sizes[rank]; j++){
            if(ctx.adjacent_list[rank][j].id != cycle_local->fish->id)
                printf("MISSMATCH ID ERROR\n");

            cycle_local->fish->eating += ctx.adjacent_list[rank][j].eating;
            cycle_local = cycle_local->next; 
        }
    }

    cycle_local = ctx.fishes->next;
    while(cycle_local!=NULL){
        cycle_local->fish->size += cycle_local->fish->eating;
        cycle_local->fish->eating = 0;
        if(!cycle_local->fish->active){
            struct node* to_remove = cycle_local;
            cycle_local = cycle_local->next;
            remove_node(to_remove);
        }
        else{
            cycle_local = cycle_local->next;
        }

    }

}

void eating_step(){
    struct node* cycle;
    struct node* current;
    current = ctx.fishes->next;

    send_neighbor();
    recv_neighbor();


    MPI_Barrier(MPI_COMM_WORLD);

    //For each fish in the local list check if there is some fish in the local list or
    //in one adjacent that can eat it
    while(current!=NULL){
        struct node* biggest_local = NULL;
        struct fish* biggest_non_local = NULL;
        cycle = ctx.fishes->next;

        //check the local list
        while(cycle != current){
            if(distance(current->fish,cycle->fish) <= params.eating_distance 
                                    && current->fish->size < cycle->fish->size){
                    biggest_local = cycle;
                    current->fish->active = 0;
                }
            cycle = cycle->next;
        }

        //check adjacent lists
        for (int i=0; i<ctx.number_of_neighbors; i++){
            int rank = ctx.neighbor_ranks[i]; 

            for (int j = 0; j<ctx.adjacent_sizes[rank]; j++){
                struct fish* cycle_fish = &ctx.adjacent_list[rank][j];
                if(distance(current->fish,cycle_fish) <= params.eating_distance 
                                    && current->fish->size < cycle_fish->size){
                    biggest_non_local = cycle_fish;
                    current->fish->active = 0;
                    break;
                }
            }    
        }
        current = current->next;

        //if there is no fish eating go to next iteration
        if(biggest_local==NULL && biggest_non_local==NULL){
            continue;
        }

        //increase the size of the biggest eating fish, if tie local wins 
        if(biggest_non_local == NULL){
            biggest_local->fish->eating += 1;
        }
        else if(biggest_local == NULL){
            biggest_non_local->eating += 1;
        }
        else{       
            if(biggest_non_local->size > biggest_local->fish->size){
                biggest_non_local->eating += 1;
            }
            else
                biggest_local->fish->eating += 1;
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    free(current);

    send_adj();
    //free adjacent list
    for(int i = 0;i<ctx.number_of_neighbors;i++)
        free(ctx.adjacent_list[ctx.neighbor_ranks[i]]);
    
    recv_neighbor(); 
    merge_neighbors();

    //free adjacent list
    for(int i = 0;i<ctx.number_of_neighbors;i++)
        free(ctx.adjacent_list[ctx.neighbor_ranks[i]]);
}

//returns the slice resposable for the position of the fish
int get_slice_from_position(struct fish* f){
    double slice_size = (float) params.edge_size/ctx.world_size;
    return (int)(f->y/slice_size);
}

//expand the array to double its size
void expand(int index, struct fish** to_send, int* sizes){
    struct fish * old_list = to_send[index];
    struct fish * new_list = malloc((sizes[index] * 2)*sizeof(struct fish));

    for (int i=0; i<sizes[index]; i++){
        new_list[i] = old_list[i];
    }

    free(old_list);
    to_send[index]= new_list;
    sizes[index] = sizes[index] * 2;
    return;
}

void add_to_slice(struct fish* f, int* index, struct fish* array){
    array[*index] = *f;
    *index = *index + 1; 
}

void move_step(){
    struct node* current = ctx.fishes->next;
    //one array of fishes for each slice because the fish could end up in every slice after moving
    struct fish* to_send[ctx.world_size];
    int indexes[ctx.world_size];
    int sizes[ctx.world_size];
    int starting_size = 500;

    for(int i=0; i<ctx.world_size; i++){
        to_send[i] = malloc(starting_size*sizeof(struct fish));
        sizes[i] = starting_size;
        indexes[i] = 0;
    }
    

    //for each fish move it and check in which slice it ended up
    //if it ended up in a slice different from the local one 
    //move it to the array that will be sent to the process responsable
    //for that slice. The arrays are basically like Java arrayList
    while(current != NULL){
        int slice;
        move_fish(current->fish);
        slice = get_slice_from_position(current->fish);
        if(slice != ctx.my_rank){

            add_to_slice(current->fish, &indexes[slice], to_send[slice]);
            if(indexes[slice]==sizes[slice])
                expand(slice,to_send,sizes);
            struct node* to_remove = current;
            current = current->next;
            remove_node(to_remove);
            continue;
        }
        current = current->next;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    //send the to_send arrays to the others
    MPI_Request send_req[(ctx.world_size-2)*2];
    int j=0;
    for(int i=0;i<ctx.world_size;i++){
        //we skip our own rank
        if(i==ctx.my_rank)
            continue;
        MPI_Isend(&indexes[i] , 1 , MPI_INT , i , 0 , MPI_COMM_WORLD , &send_req[j]);
        j++;
        MPI_Isend(to_send[i] ,indexes[i] , type_fish , i , 0 , MPI_COMM_WORLD , &send_req[j]);
        j++;;
    }
    MPI_Status stats[j];
    MPI_Waitall(j,send_req,stats);

    //receive the arrays from the other 
    MPI_Request recv_req[ctx.world_size-1];
    j=0;
    struct fish* to_recv[ctx.world_size];
    int to_recv_sizes[ctx.world_size];
    for(int i=0;i<ctx.world_size;i++){
        //we skip our own rank
        if(i==ctx.my_rank)
            continue;
        MPI_Irecv(&to_recv_sizes[i],1 , MPI_INT , i , 0 , MPI_COMM_WORLD , &recv_req[j]);
        j++;
    }
    MPI_Status stats2[j];
    MPI_Waitall(j,recv_req,stats2);

    //allocate the buffers where to receive according to the size that was just received
    for(int i=0;i<ctx.world_size;i++){
        //we skip our own rank
        if(i==ctx.my_rank)
            continue;
        to_recv[i] = malloc(sizeof(struct fish) * to_recv_sizes[i]);
    }

    j=0;
    MPI_Request recv_req2[ctx.world_size-1];
    for(int i=0;i<ctx.world_size;i++){
        //we skip our own rank
        if(i==ctx.my_rank)
            continue;
        MPI_Irecv(to_recv[i],to_recv_sizes[i],type_fish, i , 0 , MPI_COMM_WORLD , &recv_req2[j]);
        j++;
    }
    MPI_Status stats3[j];
    MPI_Waitall(j,recv_req2,stats3);

    //put the fishes received in the ctx.fishes list
    for(int i=0;i<ctx.world_size;i++){
        if(i==ctx.my_rank)
            continue;
        for(j=0;j<to_recv_sizes[i];j++){
            add(to_recv[i][j],ctx.fishes);
        }
        if(to_recv_sizes[i]!=0)
            free(to_recv[i]);
        if(indexes[i]!=0)
            free(to_send[i]);
    }
    if(params.report){
        MPI_Barrier(MPI_COMM_WORLD);
        print_local();
    }
}

int check_total_count(){

    MPI_Request recv_req[ctx.world_size-1];
    MPI_Request send_req[ctx.world_size-1];
    int j=0;
    int to_recv_counts[ctx.world_size];
    to_recv_counts[ctx.my_rank] = count_local();
    int res=0;

    for(int i=0;i<ctx.world_size;i++){
        //we skip our own rank
        if(i==ctx.my_rank)
            continue;
        MPI_Isend(&to_recv_counts[ctx.my_rank] , 1 , MPI_INT , i , 0 , MPI_COMM_WORLD , &send_req[j]);
        j++;
    }
    MPI_Status stats[j];
    MPI_Waitall(j,send_req,stats);


    j=0;
    for(int i=0;i<ctx.world_size;i++){
        //we skip our own rank
        if(i==ctx.my_rank)
            continue;
        MPI_Irecv(&to_recv_counts[i],1 , MPI_INT , i , 0 , MPI_COMM_WORLD , &recv_req[j]);
        j++;
    }
    MPI_Status stats2[j];
    MPI_Waitall(j,recv_req,stats2);

    for(int i=0;i<ctx.world_size;i++){
        res += to_recv_counts[i];
    }
    return res;
}

//returns -1 if no tie, 0 if empty, size of the tied fish if tied
int local_tie(){
    struct node* cycle;
    cycle = ctx.fishes->next;
    int res = 0;
    int max = 0;

    while(cycle!=NULL){
        if(max == 0){
            max = cycle->fish->size;
            cycle = cycle->next;
            res = max;
            continue;
        }
        if(cycle->fish->size != max){
            return -1;
        }
        else{
            cycle = cycle->next;
        }
    }
    return res;
}

int is_tie(){

    MPI_Request recv_req[ctx.world_size-1];
    MPI_Request send_req[ctx.world_size-1];
    int j=0;
    int to_recv[ctx.world_size];
    to_recv[ctx.my_rank] = local_tie();
    int res=0;

    for(int i=0;i<ctx.world_size;i++){
        //we skip our own rank
        if(i==ctx.my_rank)
            continue;
        MPI_Isend(&to_recv[ctx.my_rank] , 1 , MPI_INT , i , 0 , MPI_COMM_WORLD , &send_req[j]);
        j++;
    }
    MPI_Status stats[j];
    MPI_Waitall(j,send_req,stats);


    j=0;
    for(int i=0;i<ctx.world_size;i++){
        //we skip our own rank
        if(i==ctx.my_rank)
            continue;
        MPI_Irecv(&to_recv[i],1 , MPI_INT , i , 0 , MPI_COMM_WORLD , &recv_req[j]);
        j++;
    }
    MPI_Status stats2[j];
    MPI_Waitall(j,recv_req,stats2);


    int max = 0;
    int how_many_ties;
    for(int i=0;i<ctx.world_size;i++){

        if(to_recv[i]!=0){
            if(to_recv[i] > 0){
                if(max==0){
                    max = to_recv[i];
                    how_many_ties = 1;
                    continue;
                }
                else{
                    if(to_recv[i]!=max)
                        return 0;
                }
            }
            if(to_recv[i] == max){
                how_many_ties++;
                continue;
            }
            if(to_recv[i]==-1)
                return 0;
        }
    }
    if(how_many_ties > 1)
        return 1;
    else 
        return 0;
}

int continue_play(int total){

    int tie = is_tie();

    //printf("%d- total BEFORE TIE %d\n",ctx.my_rank,total);
    if(tie){
        if(ctx.my_rank==0)
            printf("GAME ENDED WITH A TIE\n");
        print_local();
    }
    else{
        if(total == 1){
            if(ctx.my_rank==0)
                printf("GAME ENDED WITH A WINNER\n");
            print_local();
        }
    }

    return(total>1 && !tie);

}

int get_biggest_size(){
    struct node* node = ctx.fishes->next;
    if(node == NULL)
        return -1;

    return node->fish->size;
}

int get_smallest_size(){
    struct node* node = ctx.fishes->next;
    if(node == NULL)
        return -1;

    while(node->next!=NULL)
        node = node->next;
    
    return node->fish->size;
}

void end_of_day(int iteration, int total){
    int size_of_biggest = get_biggest_size();
    int size_of_smallest = get_smallest_size();

    if(ctx.my_rank==0){
        printf("The number of fishes in the simulation at iteration %d is %d\n",iteration,total);
    }

    if(ctx.my_rank==0){
        MPI_Request recv_req[ctx.world_size-1];
        int to_recv[ctx.world_size];
        to_recv[ctx.my_rank] = size_of_biggest;
        int max = 0;
        int j=0;

        for(int i=1;i<ctx.world_size;i++){
            MPI_Irecv(&to_recv[i],1,MPI_INT,i,0,MPI_COMM_WORLD,&recv_req[j]);
            j++;
        }
        MPI_Status stats[j];
        MPI_Waitall(j,recv_req,stats);

        for(int i=0;i<ctx.world_size;i++){
            if(max<to_recv[i])
                max = to_recv[i];
        }
        printf("The biggest size in simulation at iteration %d is %d\n",iteration,max);
        size_of_biggest = max;
    }
    else{
        MPI_Request req;
        MPI_Isend(&size_of_biggest,1,MPI_INT,0,0,MPI_COMM_WORLD,&req);
        MPI_Status stat;
        MPI_Wait(&req,&stat);
    }

    if(ctx.my_rank==0){
        MPI_Request recv_req[ctx.world_size-1];
        int to_recv[ctx.world_size];
        to_recv[ctx.my_rank] = size_of_smallest;
        int min = size_of_biggest;
        int j=0;

        for(int i=1;i<ctx.world_size;i++){
            MPI_Irecv(&to_recv[i],1,MPI_INT,i,0,MPI_COMM_WORLD,&recv_req[j]);
            j++;
        }
        MPI_Status stats[j];
        MPI_Waitall(j,recv_req,stats);

        for(int i=0;i<ctx.world_size;i++){
            if(min > to_recv[i] && (to_recv[i]!=-1))
                min = to_recv[i];
        }
        printf("The smallest size in simulation at iteration %d is %d\n",iteration,min);
    }
    else{
        MPI_Request req;
        MPI_Isend(&size_of_smallest,1,MPI_INT,0,0,MPI_COMM_WORLD,&req);
        MPI_Status stat;
        MPI_Wait(&req,&stat);
    }
    
}

void play(){
    int iteration = 0;
    int total = check_total_count();

    printf("%d- starting the game\n",ctx.my_rank);
    while(continue_play(total)){
        MPI_Barrier(MPI_COMM_WORLD);
        move_step();
        MPI_Barrier(MPI_COMM_WORLD);
        eating_step();
        MPI_Barrier(MPI_COMM_WORLD);
        iteration++;
        total = check_total_count();
        MPI_Barrier(MPI_COMM_WORLD);
        end_of_day(iteration,total);
    }
    if(ctx.my_rank == 0)
        printf("Number of iterations: %d\n",iteration);

}

double randfrom(double min, double max){
    double range = (max - min); 
    double div = RAND_MAX / range;
    return min + (rand() / div);
}

struct speed create_speed(){
    struct speed s;
    s.x = randfrom(-1*params.speed, params.speed);
    s.y = randfrom(-1*params.speed, params.speed);
    s.z = randfrom(-1*params.speed, params.speed);
    return s;
}

struct fish create_fish(long id){

    struct fish f;
    
    f.x = randfrom(0,params.edge_size);
    f.y = randfrom(0,params.edge_size);
    f.z = randfrom(0,params.edge_size);
    f.id = id;
    f.active = 1;
    f.eating = 0;
    f.speed = create_speed();
    f.size = rand() % 5;
    return f;
}

void setup(){

    int world_size,rank;

    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    //creating the head of the list of fishes
    struct node* list = malloc(sizeof(struct node));
    list->head = list;
    list->next = NULL;
    list->prev = NULL;

    //Setup all the context variables
    ctx.my_rank = rank;
    srand(time(NULL) * ctx.my_rank);
    printf("%d-seed %ld\n",ctx.my_rank,time(NULL) + ctx.my_rank);
    ctx.world_size = world_size;
    ctx.end_y = (params.edge_size/ctx.world_size) * (ctx.my_rank+1);
    ctx.start_y = (params.edge_size/ctx.world_size) * (ctx.my_rank);
    ctx.adjacent_to_consider = ceil(params.eating_distance/(ctx.end_y - ctx.start_y));
    ctx.adjacent_list = malloc(sizeof(struct fish*) * (ctx.world_size));
    ctx.adjacent_sizes = malloc(sizeof(int)*(ctx.world_size));
    ctx.neighbor_ranks = malloc(sizeof(int)*ctx.adjacent_to_consider*2);

    int tmp = 0;
    for(int i=1; i<=ctx.adjacent_to_consider; i++){
        if(ctx.world_size-1 >= ctx.my_rank + i){
            ctx.neighbor_ranks[tmp] = ctx.my_rank + i;
            tmp++;
        }
        if(0<= ctx.my_rank-i){
            ctx.neighbor_ranks[tmp] = ctx.my_rank -i;
            tmp++;
        }
    }
    ctx.number_of_neighbors = tmp;

    //creating fishes
    for(int i=0; i<((int)params.numb_fish/ctx.world_size); i++){
        long id = (((int)params.numb_fish/ctx.world_size) * ctx.my_rank) + i;
        struct fish f = create_fish(id);
        add(f,list);
    }
    ctx.fishes = list;


}

void create_types_speed(){
    struct speed s;
    MPI_Aint displacement [3];
    MPI_Aint base_add;
    int lengths[3] = { 1, 1, 1 };

    MPI_Get_address(&s, &base_add);
    MPI_Get_address(&s.x, &displacement[0]);
    MPI_Get_address(&s.y, &displacement[1]);
    MPI_Get_address(&s.z, &displacement[2]);
    displacement[0] = MPI_Aint_diff(displacement[0],base_add);
    displacement[1] = MPI_Aint_diff(displacement[1],base_add);
    displacement[2] = MPI_Aint_diff(displacement[2],base_add);

    MPI_Datatype types[3]= {MPI_DOUBLE,MPI_DOUBLE,MPI_DOUBLE};
    MPI_Type_create_struct(3, lengths, displacement, types, &type_speed);
    MPI_Type_commit(&type_speed);
}

void create_type_fish(){
    struct fish f;
    MPI_Aint displacement [8];
    MPI_Aint base_add;
    int lengths[8] = { 1, 1, 1,1,1,1,1,1 };

    MPI_Get_address(&f, &base_add);
    MPI_Get_address(&f.x, &displacement[0]);
    MPI_Get_address(&f.y, &displacement[1]);
    MPI_Get_address(&f.z, &displacement[2]);
    MPI_Get_address(&f.speed, &displacement[3]);
    MPI_Get_address(&f.active, &displacement[4]);
    MPI_Get_address(&f.id, &displacement[5]);
    MPI_Get_address(&f.size, &displacement[6]);
    MPI_Get_address(&f.eating, &displacement[7]);

    displacement[0] = MPI_Aint_diff(displacement[0],base_add);
    displacement[1] = MPI_Aint_diff(displacement[1],base_add);
    displacement[2] = MPI_Aint_diff(displacement[2],base_add);
    displacement[3] = MPI_Aint_diff(displacement[3],base_add);
    displacement[4] = MPI_Aint_diff(displacement[4],base_add);
    displacement[5] = MPI_Aint_diff(displacement[5],base_add);
    displacement[6] = MPI_Aint_diff(displacement[6],base_add);
    displacement[7] = MPI_Aint_diff(displacement[7],base_add);


    MPI_Datatype types[8]= {MPI_DOUBLE,MPI_DOUBLE,MPI_DOUBLE,type_speed,MPI_CHAR,MPI_LONG,MPI_INT,MPI_INT};
    MPI_Type_create_struct(8, lengths, displacement, types, &type_fish);
    MPI_Type_commit(&type_fish);
}

void print_info(){
    printf("USAGE:\n-e <num>: edge size (default 5)\n");
    printf("-n <num>: number of fishes (default 10)\n");
    printf("-s <num>: speed (default 1)\n");
    printf("-d <num>: eating distance (default 0.5)\n");
    printf("-t <num>: timestep in seconds (default 1)\n");
}

int parseArgs(int argc, char* argv[]){
    int opt;
    struct parameters{
        int numb_fish;
        double edge_size;
        double speed;
        double eating_distance;
        int timestep;
    };
    params.numb_fish = 10;
    params.edge_size = 5;
    params.speed = 1;
    params.eating_distance = 0.5;
    params.timestep = 1;
    params.report = 0;


    while((opt = getopt(argc,argv,"hs:d:t:n:e:r")) != -1) {
        switch (opt) {
            case 'e':
                params.edge_size = atof(optarg);
                break;
            case 'n':
                params.numb_fish = atoi(optarg);
                break;
            case 's':
                params.speed = atof(optarg);
                break;
            case 'd':
                params.eating_distance = atof(optarg);
                break;
            case 't':
                params.timestep = atoi(optarg);
                break;
            case 'r':
                params.report = 1;
                break;
            default:
                print_info();
                break;
                return -1;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    // Init the MPI environment
    MPI_Init(NULL, NULL);
    create_types_speed();
    create_type_fish();

    parseArgs(argc,argv);
    printf("args parsed\n");

    setup();
    printf("setup ended for process %d\n",ctx.my_rank);
    printf("num of neighbors %d\n",ctx.number_of_neighbors);
    MPI_Barrier(MPI_COMM_WORLD);

    play();

    // Finalize the MPI environment
    MPI_Finalize();
}