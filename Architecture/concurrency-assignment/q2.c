#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

/**
 * Defined the global variables and structs. 
 * Structs: for person and team.
 * Variables: for the number of people.
 */
int h_cap, a_cap, n_cap;
sem_t h_zone, a_zone, n_zone;
int tspectating;

int people = 0;

/**
 * Defined the struct for the people.
 * Defined the struct for the teams.
 */
typedef struct person {
    int id;
    int goals;
    int treach;
    int group_id;
    int tpatience;
    char type;
    char name[20];
} person;

typedef struct team {
    float *prob_goal;
    int id;
    char name[20];
    int chances;
    int *tprev;
    char type;
} team;

person *persons;
team *teams;

/**
 * goals: number of goals that each team scored
 * l_goals: lock for goals
 * c_goals: condition variable for goals
 */
int goals[2];
pthread_t *t_people, *t_teams;
pthread_mutex_t l_goals[2];
pthread_cond_t c_goals[2];

/**
 * Variables to keep track of the zones that a person is in:
 * H: Home zone
 * A: Away zone
 * N: Neutral zone
 * D: Didn't find any zone
 * E: Entrance
 * G: Gone
 * More details of variables:
 * l_person: lock for people's zones
 * c_person: condition variable for people's zones (on change)
 * zone[<person>]: the zone that a person is in
 */
char *zone;
pthread_mutex_t *l_person;
pthread_cond_t *c_person;

/**
 * Here, we have three functions describing search across three zones.
 * Namely, we have three functions for each person:
 * 1. search_home_zone: search for home zone
 * 2. search_away_zone: search for away zone
 * 3. search_neutral_zone: search for neutral zone
 */
void *search_home_zone(void *args) {
    struct timespec ts;
    person p = *(person *)args;
    
    clock_gettime(CLOCK_REALTIME, &ts),
    ts.tv_sec += p.tpatience;

    if (errno == ETIMEDOUT) {
        if (sem_timedwait(&h_zone, &ts) == -1) {
            pthread_mutex_lock(&l_person[p.id]);
            if(zone[p.id] == 'E')
                zone[p.id] = 'D';
            pthread_mutex_unlock(&l_person[p.id]);  
            return NULL;
        }
    }

    pthread_mutex_lock(&l_person[p.id]);
    zone[p.id] = zone[p.id] == 'E' ? 'X' : (zone[p.id] == 'D' ? 'X' : zone[p.id]);
    if (zone[p.id] == 'X')
        zone[p.id] = 'H',
        printf("\x1b[32m" "%s (%c) got a seat in zone H" "\x1b[0m" "\n", p.name, p.type);
    else
        sem_post(&h_zone);
    pthread_mutex_unlock(&l_person[p.id]);

    return NULL;    
}

void *search_away_zone(void *args) {
    struct timespec ts;
    person p = *(person *)args;
    
    clock_gettime(CLOCK_REALTIME, &ts), 
    ts.tv_sec += p.tpatience;

    if (errno == ETIMEDOUT) {
        if (sem_timedwait(&a_zone, &ts) == -1) {
            pthread_mutex_lock(&l_person[p.id]);
            if(zone[p.id] == 'E')
                zone[p.id] = 'D';
            pthread_mutex_unlock(&l_person[p.id]);  
            return NULL;
        }
    }

    pthread_mutex_lock(&l_person[p.id]);
    zone[p.id] = zone[p.id] == 'E' ? 'X' : (zone[p.id] == 'D' ? 'X' : zone[p.id]);
    if(zone[p.id] == 'X')
        zone[p.id] = 'A',
        printf("\x1b[32m" "%s (%c) got a seat in zone A" "\x1b[0m" "\n", p.name, p.type);
    else
        sem_post(&a_zone);
    pthread_mutex_unlock(&l_person[p.id]);

    return NULL;    
}

void *search_neutral_zone(void *args) {
    struct timespec ts;
    person p = *(person *)args;
    
    clock_gettime(CLOCK_REALTIME, &ts),
    ts.tv_sec += p.tpatience;

    if (errno == ETIMEDOUT) {
        if (sem_timedwait(&n_zone, &ts) == -1) {
            pthread_mutex_lock(&l_person[p.id]);
            if(zone[p.id] == 'E')
                zone[p.id] = 'D';
            pthread_mutex_unlock(&l_person[p.id]);  
            return NULL;
        }
    }

    pthread_mutex_lock(&l_person[p.id]);
    zone[p.id] = zone[p.id] == 'E' ? 'X' : (zone[p.id] == 'D' ? 'X' : zone[p.id]);
    if(zone[p.id] == 'X')
        zone[p.id] = 'N',
        printf("\x1b[32m" "%s (%c) got a seat in zone N" "\x1b[0m" "\n", p.name, p.type);
    else
        sem_post(&n_zone);
    pthread_mutex_unlock(&l_person[p.id]);

    return NULL;    
}

/**
 * We are simulating seating arrangement here.
 * Within the function, we call the above declared functions to search for zones. 
 */
void *simulate_person_seat(void *args) {
    person p = *(person *)args;
    pthread_t t_h, t_a, t_n;
    
    if (p.type == 'A') {
        pthread_create(&t_a, NULL, search_away_zone, &p);
        pthread_join(t_a, NULL);
    }
    else if (p.type == 'H') {
        pthread_create(&t_h, NULL, search_home_zone, &p);
        pthread_create(&t_n, NULL, search_neutral_zone, &p);
        pthread_join(t_h, NULL);
        pthread_join(t_n, NULL);
    }
    else if (p.type == 'N') {
        pthread_create(&t_h, NULL, search_home_zone, &p);
        pthread_create(&t_a, NULL, search_away_zone, &p);
        pthread_create(&t_n, NULL, search_neutral_zone, &p);
        pthread_join(t_h, NULL);
        pthread_join(t_a, NULL);
        pthread_join(t_n, NULL);
    }

    return NULL;
}

void *simulate_person_in_game(void *args) {
    person p_person = *(person *)args;

    // if the person is a H fan, then look at the goals of the Away team
    if(p_person.type - 'H' == 0){
        pthread_mutex_lock(&l_goals[1]);
        // wait until the goals of the opposing team doesn't enrage
        // thsi person, and until the match isn't over
        while(goals[1] < p_person.goals && goals[1] >= 0) {
            pthread_cond_wait(&c_goals[1], &l_goals[1]);
        }
        if(goals[1] >= p_person.goals){
            printf("\x1b[31m" "%s got enraged and went to the exit" "\x1b[0m" "\n", p_person.name);
        }
        pthread_mutex_unlock(&l_goals[1]);
        pthread_cond_broadcast(&c_person[p_person.id]);
    }
    // if the person is a A fan, then look at the goals of the Home team
    else if(p_person.type - 'A' == 0){
        pthread_mutex_lock(&l_goals[0]);
        // wait until the goals of the opposing team doesn't enrage
        // thsi person, and until the match isn't over
        while(goals[0] < p_person.goals && goals[0] >= 0) {
            pthread_cond_wait(&c_goals[0], &l_goals[0]);
        }
        if(goals[0] >= p_person.goals){
            printf("\x1b[31m" "%s got enraged and went to the exit" "\x1b[0m" "\n", p_person.name);
        }
        pthread_mutex_unlock(&l_goals[0]);
        pthread_cond_broadcast(&c_person[p_person.id]);
    }
    
    return NULL;
}

void *simulate_person_enter_exit(void *args) {
    person p_person = *(person *)args;

    // person comes to the gate at time reach_time
    sleep(p_person.treach);

    printf("\x1b[31m" "%s has reached the stadium" "\x1b[0m" "\n", p_person.name);

    // run the thread where the person is simulated to wait for a seat in a zone and watch the match
    pthread_t thread;
    pthread_create(&thread, NULL, simulate_person_seat, &p_person);
    pthread_join(thread, NULL);

    // if the person did not find any seat
    pthread_mutex_lock(&l_person[p_person.id]);
    if(zone[p_person.id] - 'D' == 0 || zone[p_person.id] - 'G' == 0){
        printf("\x1b[35m" "%s did not find a seat in any of the zones" "\x1b[0m" "\n", p_person.name);
        pthread_mutex_unlock(&l_person[p_person.id]);
        printf("\x1b[34m" "%s left the stadium" "\x1b[0m" "\n", p_person.name);
        return NULL;
    }
    pthread_mutex_unlock(&l_person[p_person.id]);

    // simulate the person inside the game
    pthread_create(&thread, NULL, simulate_person_in_game, &p_person);

    // set timespec for the spectating time
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += tspectating;

    pthread_mutex_lock(&l_person[p_person.id]);
    // simulate_person will signal the conditional variable if the person wants to leave
    if(pthread_cond_timedwait(&c_person[p_person.id], &l_person[p_person.id], &ts) == ETIMEDOUT){
        printf("\x1b[33m" "%s watched the match for %d seconds and is leaving" "\x1b[0m" "\n", p_person.name, tspectating);
    }
    pthread_mutex_unlock(&l_person[p_person.id]);

    // Once the person wants to leave, increment the semaphore
    pthread_mutex_lock(&l_person[p_person.id]);
    if(zone[p_person.id] - 'H' == 0){
        sem_post(&h_zone);
    }
    if(zone[p_person.id] - 'A' == 0){
        sem_post(&a_zone);
    }
    if(zone[p_person.id] - 'N' == 0){
        sem_post(&n_zone);
    }
    zone[p_person.id] = 'G';
    pthread_mutex_unlock(&l_person[p_person.id]);

    printf("\x1b[34m" "%s left the stadium" "\x1b[0m" "\n", p_person.name);

    return NULL;
}

void *simulate_team(void *args) {
    team p_team = *(team *)args;
    
    for(int i = 0; i < p_team.chances; i++){
        // sleep until the time elapses till the next chance to score a goal
        sleep(p_team.tprev[i]);

        // choose whether the team has scored a goal or not
        float x = (float)rand()/(float)(RAND_MAX/1.0);
        if(x < p_team.prob_goal[i]){
            // team has scored a goal

            pthread_mutex_lock(&l_goals[p_team.id]);
            goals[p_team.id]++;
            printf("\x1b[36m" "Team %c has scored goal number %d" "\x1b[0m" "\n", p_team.type, goals[p_team.id]);
            pthread_mutex_unlock(&l_goals[p_team.id]);

        }
        else{
            printf("\x1b[36m" "Team %c missed their chance to score a goal" "\x1b[0m" "\n", p_team.type);
        }
        // broadcast after every goal scoring chance, even if the team did not score
        pthread_cond_broadcast(&c_goals[p_team.id]);
    }

    return NULL;
}

int main() 
{
    scanf("%d %d %d", &h_cap, &a_cap, &n_cap);
    scanf("%d", &tspectating);

    persons = malloc(sizeof(person) * 1);

    int num_groups, num_in_group;
    scanf("%d", &num_groups);

    for(int i = 1; i <= num_groups; i++){
        scanf("%d", &num_in_group);

        for(int ii = 0; ii < num_in_group; ii++){
            persons = realloc(persons, sizeof(person) * (people+1));

            scanf("%s %c %d %d %d", persons[people].name, &persons[people].type, &persons[people].treach, &persons[people].tpatience, &persons[people].goals);
            persons[people].group_id = i;
            persons[people].id = people;

            people++;
        }
    }


    teams = malloc(sizeof(team) * 2);
    teams[0].type = 'H';
    teams[0].chances=0;
    teams[0].id = 0;
    teams[1].id = 1;
    teams[1].type = 'A';
    teams[1].chances=0;
    teams[0].tprev = malloc(sizeof(int) * 1);
    teams[1].tprev = malloc(sizeof(int) * 1);
    teams[0].prob_goal = malloc(sizeof(float) * 1);
    teams[1].prob_goal = malloc(sizeof(float) * 1);

    for(int i = 0; i < 2; i++){
        pthread_mutex_init(&l_goals[i], NULL);
        pthread_cond_init(&c_goals[i], NULL);
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
            teams[0].tprev = realloc(teams[0].tprev, sizeof(int) * (teams[0].chances+1));
            teams[0].prob_goal = realloc(teams[0].prob_goal, sizeof(float) * (teams[0].chances+1));

            scanf("%d %f\n", &inp_time, &teams[0].prob_goal[teams[0].chances]);
            // store the time from the previous chance to goal
            teams[0].tprev[teams[0].chances] = inp_time-prev_h_time;
            prev_h_time = inp_time;

            teams[0].chances++;
        }
        else if (inp_team - 'A' == 0){
            teams[1].tprev = realloc(teams[1].tprev, sizeof(int) * (teams[1].chances+1));
            teams[1].prob_goal = realloc(teams[1].prob_goal, sizeof(float) * (teams[1].chances+1));

            scanf("%d %f\n", &inp_time, &teams[1].prob_goal[teams[1].chances]);
            // store the time from the previous chance to goal
            teams[1].tprev[teams[1].chances] = inp_time-prev_a_time;
            prev_a_time = inp_time;

            teams[1].chances++;
        }
    }


    // initialize the semaphores corresponding to the number of seats in each zone
    sem_init(&h_zone, 0, h_cap);
    sem_init(&a_zone, 0, a_cap);
    sem_init(&n_zone, 0, n_cap);

    c_person = malloc(sizeof(pthread_cond_t) * people);
    zone = malloc(sizeof(char) * people);
    l_person = malloc(sizeof(pthread_mutex_t) * people);
    for (int i = 0; i < people; i++){
        pthread_cond_init(&c_person[i], NULL);
        pthread_mutex_init(&l_person[i], NULL);
        zone[i] = 'E';
    }


    // allocate space and create threads
    t_people = malloc(sizeof(pthread_t) * people);
    t_teams = malloc(sizeof(pthread_t) * 2);
    for (int i = 0; i < people; i++){
        pthread_create(&t_people[i], NULL, simulate_person_enter_exit, &persons[i]);
    }
    for (int i = 0; i < 2; i++){
        pthread_create(&t_teams[i], NULL, simulate_team, &teams[i]);
    }


    // wait for the threads to finish
    for (int i = 0; i < people; i++){
        pthread_join(t_people[i], NULL);
    }
    for (int i = 0; i < 2; i++){
        pthread_join(t_teams[i], NULL);
    }


    for(int i = 0; i < 2; i++){
        goals[i] = -1;
        pthread_cond_broadcast(&c_goals[i]);
        pthread_mutex_destroy(&l_goals[i]);
        pthread_cond_destroy(&c_goals[i]);
        pthread_cancel(t_teams[i]);
    }
    sem_destroy(&h_zone);
    sem_destroy(&a_zone);
    sem_destroy(&n_zone);

    return 0;
}