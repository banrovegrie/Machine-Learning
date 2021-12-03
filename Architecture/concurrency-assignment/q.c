#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct person {
    char name[20];
    char fan_type;
    int id;
    int group_no;
    int reach_time;
    int patience_time;
    int num_goals;
} person;

typedef struct team {
    char team_type;
    int id;
    int num_chances;
    int *time_from_previous_chance;
    float *probability_goal;
} team;

#include <pthread.h>
#include <semaphore.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

int h_capacity, a_capacity, n_capacity;
sem_t h_zone, a_zone, n_zone;
int spectating_time;

int num_people=0;

person *people;
team *teams;

pthread_t *t_people, *t_teams;

// variables of the number of goals that each team has scored
pthread_mutex_t goals_lock[2];
int goals[2];
pthread_cond_t goals_changed[2];

// variables to keep track of the zones that a person is in: H,A,N,D(Didn't find any zone),E(Entrance),G(Gone)
pthread_cond_t *person_moved;
char *person_in_zone;
pthread_mutex_t *person_zone_lock;

void *search_in_h_zone(void *args) {
    person p_person = *(person *)args;

    // store the patience time of the person
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += p_person.patience_time;

    // wait for a seat to be allocated in h_zone for the patience time
    if (sem_timedwait(&h_zone, &ts) == -1 && errno == ETIMEDOUT) {
        pthread_mutex_lock(&person_zone_lock[p_person.id]);
        // if the person was at the entrance till now, change the zone info so that he didn't get any zone
        if(person_in_zone[p_person.id] - 'E' == 0){
            person_in_zone[p_person.id] = 'D';
        }
        pthread_mutex_unlock(&person_zone_lock[p_person.id]);  
        return NULL;
    }

    pthread_mutex_lock(&person_zone_lock[p_person.id]);
    // if the person was at the entrance or didn't get any seat from the other zone threads, allocate this zone
    if(person_in_zone[p_person.id] - 'E' == 0 || person_in_zone[p_person.id] - 'D' == 0){
        person_in_zone[p_person.id] = 'H';
        printf(ANSI_COLOR_GREEN "%s (%c) got a seat in zone H" ANSI_COLOR_RESET "\n", p_person.name, p_person.fan_type);
    }
    // if the person got allocated some zone, or left even, increment the semaphore since the person didn't use a seat
    else {
        sem_post(&h_zone);
    }
    pthread_mutex_unlock(&person_zone_lock[p_person.id]);

    return NULL;    
}

void *search_in_a_zone(void *args) {
    person p_person = *(person *)args;

    // store the patience time of the person
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += p_person.patience_time;

    // wait for a seat to be allocated in h_zone for the patience time
    if (sem_timedwait(&a_zone, &ts) == -1 && errno == ETIMEDOUT) {
        pthread_mutex_lock(&person_zone_lock[p_person.id]);
        // if the person was at the entrance till now, change the zone info so that he didn't get any zone
        if(person_in_zone[p_person.id] - 'E' == 0){
            person_in_zone[p_person.id] = 'D';
        }
        pthread_mutex_unlock(&person_zone_lock[p_person.id]);  
        return NULL;
    }

    pthread_mutex_lock(&person_zone_lock[p_person.id]);
    // if the person was at the entrance or didn't get any seat from the other zone threads, allocate this zone
    if(person_in_zone[p_person.id] - 'E' == 0 || person_in_zone[p_person.id] - 'D' == 0){
        person_in_zone[p_person.id] = 'A';
        printf(ANSI_COLOR_GREEN "%s (%c) got a seat in zone A" ANSI_COLOR_RESET "\n", p_person.name, p_person.fan_type);
    }
    // if the person got allocated some zone, or left even, increment the semaphore since the person didn't use a seat
    else {
        sem_post(&a_zone);
    }
    pthread_mutex_unlock(&person_zone_lock[p_person.id]);

    return NULL;    
}

void *search_in_n_zone(void *args) {
    person p_person = *(person *)args;

    // store the patience time of the person
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += p_person.patience_time;

    // wait for a seat to be allocated in h_zone for the patience time
    if (sem_timedwait(&n_zone, &ts) == -1 && errno == ETIMEDOUT) {
        pthread_mutex_lock(&person_zone_lock[p_person.id]);
        // if the person was at the entrance till now, change the zone info so that he didn't get any zone
        if(person_in_zone[p_person.id] - 'E' == 0){
            person_in_zone[p_person.id] = 'D';
        }
        pthread_mutex_unlock(&person_zone_lock[p_person.id]);  
        return NULL;
    }

    pthread_mutex_lock(&person_zone_lock[p_person.id]);
    // if the person was at the entrance or didn't get any seat from the other zone threads, allocate this zone
    if(person_in_zone[p_person.id] - 'E' == 0 || person_in_zone[p_person.id] - 'D' == 0){
        person_in_zone[p_person.id] = 'N';
        printf(ANSI_COLOR_GREEN "%s (%c) got a seat in zone N" ANSI_COLOR_RESET "\n", p_person.name, p_person.fan_type);
    }
    // if the person got allocated some zone, or left even, increment the semaphore since the person didn't use a seat
    else {
        sem_post(&n_zone);
    }
    pthread_mutex_unlock(&person_zone_lock[p_person.id]);

    return NULL;    
}

void *simulate_person_seat(void *args) {
    person p_person = *(person *)args;

    // make multiple threads based on what zones to look for, and wait for those threads to end
    if (p_person.fan_type - 'H' == 0){
        pthread_t h_thread, n_thread;
        pthread_create(&h_thread, NULL, search_in_h_zone, &p_person);
        pthread_create(&n_thread, NULL, search_in_n_zone, &p_person);
        pthread_join(h_thread, NULL);
        pthread_join(n_thread, NULL);
    }
    else if (p_person.fan_type - 'A' == 0){
        pthread_t a_thread;
        pthread_create(&a_thread, NULL, search_in_a_zone, &p_person);
        pthread_join(a_thread, NULL);
    }
    else if (p_person.fan_type - 'N' == 0){
        pthread_t h_thread, a_thread, n_thread;
        pthread_create(&h_thread, NULL, search_in_h_zone, &p_person);
        pthread_create(&a_thread, NULL, search_in_a_zone, &p_person);
        pthread_create(&n_thread, NULL, search_in_n_zone, &p_person);
        pthread_join(h_thread, NULL);
        pthread_join(a_thread, NULL);
        pthread_join(n_thread, NULL);
    }

    return NULL;
}

void *simulate_person_in_game(void *args) {
    person p_person = *(person *)args;

    // if the person is a H fan, then look at the goals of the Away team
    if(p_person.fan_type - 'H' == 0){
        pthread_mutex_lock(&goals_lock[1]);
        // wait until the goals of the opposing team doesn't enrage
        // thsi person, and until the match isn't over
        while(goals[1] < p_person.num_goals && goals[1] >= 0) {
            pthread_cond_wait(&goals_changed[1], &goals_lock[1]);
        }
        if(goals[1] >= p_person.num_goals){
            printf(ANSI_COLOR_RED "%s got enraged and went to the exit" ANSI_COLOR_RESET "\n", p_person.name);
        }
        pthread_mutex_unlock(&goals_lock[1]);
        pthread_cond_broadcast(&person_moved[p_person.id]);
    }
    // if the person is a A fan, then look at the goals of the Home team
    else if(p_person.fan_type - 'A' == 0){
        pthread_mutex_lock(&goals_lock[0]);
        // wait until the goals of the opposing team doesn't enrage
        // thsi person, and until the match isn't over
        while(goals[0] < p_person.num_goals && goals[0] >= 0) {
            pthread_cond_wait(&goals_changed[0], &goals_lock[0]);
        }
        if(goals[0] >= p_person.num_goals){
            printf(ANSI_COLOR_RED "%s got enraged and went to the exit" ANSI_COLOR_RESET "\n", p_person.name);
        }
        pthread_mutex_unlock(&goals_lock[0]);
        pthread_cond_broadcast(&person_moved[p_person.id]);
    }
    
    return NULL;
}

void *simulate_person_enter_exit(void *args) {
    person p_person = *(person *)args;

    // person comes to the gate at time reach_time
    sleep(p_person.reach_time);

    printf(ANSI_COLOR_RED "%s has reached the stadium" ANSI_COLOR_RESET "\n", p_person.name);

    // run the thread where the person is simulated to wait for a seat in a zone and watch the match
    pthread_t thread;
    pthread_create(&thread, NULL, simulate_person_seat, &p_person);
    pthread_join(thread, NULL);

    // if the person did not find any seat
    pthread_mutex_lock(&person_zone_lock[p_person.id]);
    if(person_in_zone[p_person.id] - 'D' == 0 || person_in_zone[p_person.id] - 'G' == 0){
        printf(ANSI_COLOR_BLUE "%s did not find a seat in any of the zones" ANSI_COLOR_RESET "\n", p_person.name);
        pthread_mutex_unlock(&person_zone_lock[p_person.id]);
        printf(ANSI_COLOR_MAGENTA "%s left the stadium" ANSI_COLOR_RESET "\n", p_person.name);
        return NULL;
    }
    pthread_mutex_unlock(&person_zone_lock[p_person.id]);

    // simulate the person inside the game
    pthread_create(&thread, NULL, simulate_person_in_game, &p_person);

    // set timespec for the spectating time
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += spectating_time;

    pthread_mutex_lock(&person_zone_lock[p_person.id]);
    // simulate_person will signal the conditional variable if the person wants to leave
    if(pthread_cond_timedwait(&person_moved[p_person.id], &person_zone_lock[p_person.id], &ts) == ETIMEDOUT){
        printf(ANSI_COLOR_YELLOW "%s watched the match for %d seconds and is leaving" ANSI_COLOR_RESET "\n", p_person.name, spectating_time);
    }
    pthread_mutex_unlock(&person_zone_lock[p_person.id]);

    // Once the person wants to leave, increment the semaphore
    pthread_mutex_lock(&person_zone_lock[p_person.id]);
    if(person_in_zone[p_person.id] - 'H' == 0){
        sem_post(&h_zone);
    }
    if(person_in_zone[p_person.id] - 'A' == 0){
        sem_post(&a_zone);
    }
    if(person_in_zone[p_person.id] - 'N' == 0){
        sem_post(&n_zone);
    }
    person_in_zone[p_person.id] = 'G';
    pthread_mutex_unlock(&person_zone_lock[p_person.id]);

    printf(ANSI_COLOR_MAGENTA "%s left the stadium" ANSI_COLOR_RESET "\n", p_person.name);

    return NULL;
}

void *simulate_team(void *args) {
    team p_team = *(team *)args;
    
    for(int i = 0; i < p_team.num_chances; i++){
        // sleep until the time elapses till the next chance to score a goal
        sleep(p_team.time_from_previous_chance[i]);

        // choose whether the team has scored a goal or not
        float x = (float)rand()/(float)(RAND_MAX/1.0);
        if(x < p_team.probability_goal[i]){
            // team has scored a goal

            pthread_mutex_lock(&goals_lock[p_team.id]);
            goals[p_team.id]++;
            printf(ANSI_COLOR_CYAN "Team %c has scored goal number %d" ANSI_COLOR_RESET "\n", p_team.team_type, goals[p_team.id]);
            pthread_mutex_unlock(&goals_lock[p_team.id]);

        }
        else{
            printf(ANSI_COLOR_CYAN "Team %c missed their chance to score a goal" ANSI_COLOR_RESET "\n", p_team.team_type);
        }
        // broadcast after every goal scoring chance, even if the team did not score
        pthread_cond_broadcast(&goals_changed[p_team.id]);
    }

    return NULL;
}

int main() {

    #pragma region INPUT
    scanf("%d %d %d", &h_capacity, &a_capacity, &n_capacity);
    scanf("%d", &spectating_time);

    people = malloc(sizeof(person) * 1);

    int num_groups, num_in_group;
    scanf("%d", &num_groups);

    for(int i = 1; i <= num_groups; i++){
        scanf("%d", &num_in_group);

        for(int ii = 0; ii < num_in_group; ii++){
            people = realloc(people, sizeof(person) * (num_people+1));

            scanf("%s %c %d %d %d", people[num_people].name, &people[num_people].fan_type, &people[num_people].reach_time, &people[num_people].patience_time, &people[num_people].num_goals);
            people[num_people].group_no = i;
            people[num_people].id = num_people;

            num_people++;
        }
    }


    teams = malloc(sizeof(team) * 2);
    teams[0].team_type = 'H';
    teams[0].num_chances=0;
    teams[0].id = 0;
    teams[1].id = 1;
    teams[1].team_type = 'A';
    teams[1].num_chances=0;
    teams[0].time_from_previous_chance = malloc(sizeof(int) * 1);
    teams[1].time_from_previous_chance = malloc(sizeof(int) * 1);
    teams[0].probability_goal = malloc(sizeof(float) * 1);
    teams[1].probability_goal = malloc(sizeof(float) * 1);

    for(int i = 0; i < 2; i++){
        pthread_mutex_init(&goals_lock[i], NULL);
        pthread_cond_init(&goals_changed[i], NULL);
    }
    goals[0] = 0;
    goals[1] = 0;

    int goal_scoring_chances, prev_a_time=0, prev_h_time=0;
    int inp_time;
    char inp_team;
    scanf("%d\n", &goal_scoring_chances);
    for(int i = 0; i < goal_scoring_chances; i++){
        scanf("%c", &inp_team);
        if (inp_team - 'H' == 0){
            teams[0].time_from_previous_chance = realloc(teams[0].time_from_previous_chance, sizeof(int) * (teams[0].num_chances+1));
            teams[0].probability_goal = realloc(teams[0].probability_goal, sizeof(float) * (teams[0].num_chances+1));

            scanf("%d %f\n", &inp_time, &teams[0].probability_goal[teams[0].num_chances]);
            // store the time from the previous chance to goal
            teams[0].time_from_previous_chance[teams[0].num_chances] = inp_time-prev_h_time;
            prev_h_time = inp_time;

            teams[0].num_chances++;
        }
        else if (inp_team - 'A' == 0){
            teams[1].time_from_previous_chance = realloc(teams[1].time_from_previous_chance, sizeof(int) * (teams[1].num_chances+1));
            teams[1].probability_goal = realloc(teams[1].probability_goal, sizeof(float) * (teams[1].num_chances+1));

            scanf("%d %f\n", &inp_time, &teams[1].probability_goal[teams[1].num_chances]);
            // store the time from the previous chance to goal
            teams[1].time_from_previous_chance[teams[1].num_chances] = inp_time-prev_a_time;
            prev_a_time = inp_time;

            teams[1].num_chances++;
        }
    }

    // for(int i = 0; i < 2; i++){
    //     for(int ii = 0; ii < teams[i].num_chances; ii++){
    //         printf("%c   T: %d    P: %f\n", teams[i].team_type, teams[i].time_from_previous_chance[ii], teams[i].probability_goal[ii]);
    //     }
    // }

    #pragma endregion INPUT

    // initialize the semaphores corresponding to the number of seats in each zone
    sem_init(&h_zone, 0, h_capacity);
    sem_init(&a_zone, 0, a_capacity);
    sem_init(&n_zone, 0, n_capacity);

    person_moved = malloc(sizeof(pthread_cond_t) * num_people);
    person_in_zone = malloc(sizeof(char) * num_people);
    person_zone_lock = malloc(sizeof(pthread_mutex_t) * num_people);
    for (int i = 0; i < num_people; i++){
        pthread_cond_init(&person_moved[i], NULL);
        pthread_mutex_init(&person_zone_lock[i], NULL);
        person_in_zone[i] = 'E';
    }


    // allocate space and create threads
    t_people = malloc(sizeof(pthread_t) * num_people);
    t_teams = malloc(sizeof(pthread_t) * 2);
    for (int i = 0; i < num_people; i++){
        pthread_create(&t_people[i], NULL, simulate_person_enter_exit, &people[i]);
    }
    for (int i = 0; i < 2; i++){
        pthread_create(&t_teams[i], NULL, simulate_team, &teams[i]);
    }


    // wait for the threads to finish
    for (int i = 0; i < num_people; i++){
        pthread_join(t_people[i], NULL);
    }
    for (int i = 0; i < 2; i++){
        pthread_join(t_teams[i], NULL);
    }


    for(int i = 0; i < 2; i++){
        goals[i] = -1;
        pthread_cond_broadcast(&goals_changed[i]);
        pthread_mutex_destroy(&goals_lock[i]);
        pthread_cond_destroy(&goals_changed[i]);
        pthread_cancel(t_teams[i]);
    }
    sem_destroy(&h_zone);
    sem_destroy(&a_zone);
    sem_destroy(&n_zone);

    free(person_moved);
    free(person_in_zone);
    free(person_zone_lock);

    free(people);
    free(teams[0].time_from_previous_chance);
    free(teams[0].probability_goal);
    free(teams[1].time_from_previous_chance);
    free(teams[1].probability_goal);
    free(teams);

    return 0;
}