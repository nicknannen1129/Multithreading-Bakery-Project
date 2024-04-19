#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

void random_sleep(double a, double b);

#define NUM_ITERATIONS 10
#define NUM_LEFT_OVEN_MITTS 3
#define NUM_RIGHT_OVEN_MITTS 3

//Helper function to sleep a random number of microseconds
//picked between two bounds (provided in seconds)
//pass 0.2 and 0.5 into this function as arguments
void random_sleep(double lbound_sec, double ubound_sec) {
   int num_usec;
   num_usec = lbound_sec*1000000 +
              (int)((ubound_sec - lbound_sec)*1000000 * ((double)(rand()) / RAND_MAX));
   usleep(num_usec);
   return;
}

struct arguments{
  pthread_mutex_t * right_lock;
  pthread_mutex_t * left_lock;
  pthread_mutex_t * mutex;
  pthread_cond_t * right_availible;
  pthread_cond_t * left_availible;
  int * num_availible_right_mitts;
  int * num_availible_left_mitts;
  int num_batches;
  int baker_type;
  int baker_id;
  int tid;
};

void *baker( void *arg );
void work( int type, int baker_id );
int getRightMitt( int type, int baker_id, int * num_availible_right_mitts,
              pthread_mutex_t * right_lock, pthread_mutex_t * mutex, pthread_cond_t * right_availible );
int getLeftMitt( int type, int baker_id, int * num_availible_left_mitts,
              pthread_mutex_t * left_lock, pthread_mutex_t * mutex, pthread_cond_t * left_availible );
int returnRightMitt(int type, int baker_id, int * num_availible_right_mitts,
                pthread_mutex_t * right_lock, pthread_mutex_t * mutex, pthread_cond_t * right_availible);
int returnLeftMitt(int type, int baker_id, int * num_availible_left_mitts,
                pthread_mutex_t * left_lock, pthread_mutex_t * mutex, pthread_cond_t * left_availible );
void bake(int type, int baker_id );

/*
 * Main function
 */
int main(int argc, char **argv) {

  int num_left_handed_bakers;
  int num_right_handed_bakers;
  int num_cautious_bakers;
  int seed;
  int num_availible_right_mitts = NUM_RIGHT_OVEN_MITTS;
  int num_availible_left_mitts = NUM_LEFT_OVEN_MITTS;

  pthread_mutex_t right_lock, left_lock, mutex;
  pthread_mutex_init( &mutex, NULL );
  pthread_mutex_init( &right_lock, NULL );
  pthread_mutex_init( &left_lock, NULL );
  pthread_cond_t right_availible;
  pthread_cond_t left_availible;

  pthread_cond_init( &right_availible, NULL );
  pthread_cond_init( &left_availible, NULL );

  /* Process command-line arguments */
  if (argc != 5) {
    fprintf(stderr,"Usage: %s <# left-handed bakers> <# right-handed bakers> <# cautious bakers> <seed>\n",argv[0]);
    exit(1);
  }

  if ((sscanf(argv[1],"%d",&num_left_handed_bakers) != 1) ||
      (sscanf(argv[2],"%d",&num_right_handed_bakers) != 1) ||
      (sscanf(argv[3],"%d",&num_cautious_bakers) != 1) ||
      (sscanf(argv[4],"%d",&seed) != 1) ||
      (num_left_handed_bakers < 1) ||
      (num_right_handed_bakers < 1) ||
      (num_cautious_bakers < 1) ||
      (seed < 0)) {
    fprintf(stderr,"Invalid command-line arguments... Aborting\n");
    exit(1);
  }

  /* Seed the RNG */
  srand(seed);

  // IMPLEMENT CODE HERE
  int num_threads = num_left_handed_bakers + num_right_handed_bakers
                    + num_cautious_bakers;
  pthread_t worker_thread[ num_threads ];

  struct arguments * args=(struct arguments *)malloc(sizeof(struct arguments)*num_threads);

  int thread_index;
  int left_baker_id = 0;
  int right_baker_id = 0;
  int cautious_baker_id = 0;

  for( thread_index = 0; thread_index < num_threads; thread_index++ )
  {
    args[thread_index].right_lock= &right_lock;
    args[thread_index].left_lock = &left_lock;
    args[thread_index].mutex = &mutex;
    args[thread_index].right_availible = &right_availible;
    args[thread_index].left_availible = &left_availible;
    args[thread_index].num_availible_right_mitts = &num_availible_right_mitts;
    args[thread_index].num_availible_left_mitts = &num_availible_left_mitts;
    args[thread_index].num_batches = 0;
    args[thread_index].tid = thread_index;

    // set baker's types where 1 = left handed, 2 = right handed, and 3 = cautious
    if( thread_index < num_left_handed_bakers )
    {
      args[thread_index].baker_type = 1;
      args[thread_index].baker_id = left_baker_id;
      left_baker_id++;
    }
    else if( thread_index > num_left_handed_bakers &&
      thread_index <= num_left_handed_bakers + num_right_handed_bakers)
    {
      args[thread_index].baker_type = 2;
      args[thread_index].baker_id = right_baker_id;
      right_baker_id++;
    }
    else
    {
      args[thread_index].baker_type = 3;
      args[thread_index].baker_id = cautious_baker_id;
      cautious_baker_id++;
    }

    if (pthread_create(&worker_thread[thread_index], NULL,
                      baker, (void *) &args[thread_index])) {
      fprintf(stderr,"Error while creating thread #%d\n",thread_index);
      exit(1);
    }
  }

  // Joining with child threads
  for (thread_index=0; thread_index < num_threads; thread_index++) {
    if (pthread_join(worker_thread[thread_index], NULL)) {
      fprintf(stderr,"Error while joining with child thread #%d\n",thread_index);
      exit(1);
    }
  }

  exit(0);
}

void *baker( void *arg )
{
  // unpacking argument
  struct arguments * myargs = (struct arguments*) arg;

  int baker_type = myargs->baker_type;
  int baker_id = myargs->baker_id;
  int my_tid = myargs->tid;
  int num_batches = myargs->num_batches;
  int * num_availible_right_mitts = myargs->num_availible_right_mitts;
  int * num_availible_left_mitts = myargs->num_availible_left_mitts;

  pthread_mutex_t * right_lock = myargs->right_lock;
  pthread_mutex_t * left_lock = myargs->left_lock;
  pthread_mutex_t * mutex = myargs->mutex;

  pthread_cond_t * right_availible = myargs->right_availible;
  pthread_cond_t * left_availible = myargs->left_availible;

  int has_right_mitt = 0;
  int has_left_mitt = 0;

  // loop while each thread hasn't done more than 10 batches of cookies
  for( num_batches = 0; num_batches < 10; num_batches++ )
  {
    // each thread works
    work( baker_type, baker_id );

    // if the baker is left handed, get a left mitt and bake
    if( baker_type == 1 )
    {
      has_left_mitt = getLeftMitt( baker_type, baker_id, num_availible_left_mitts + 1,
                                   left_lock, mutex, left_availible );

      if( has_left_mitt == 1 )
      {
        bake( baker_type, baker_id );
        has_left_mitt = returnLeftMitt( baker_type, baker_id, num_availible_left_mitts + 1,
                                        left_lock, mutex, left_availible );
      }
    }

    // if the baker is right handed, get a right mitt and bake
    if( baker_type == 2)
    {
      has_right_mitt = getRightMitt( baker_type, baker_id, num_availible_right_mitts + 1,
                                     right_lock, mutex, right_availible );

      if( has_right_mitt == 1 )
      {
        bake( baker_type, baker_id );
        has_right_mitt = returnRightMitt( baker_type, baker_id, num_availible_right_mitts + 1,
                                          right_lock, mutex, right_availible );
      }
    }

    // if the baker is cautious, get both mitts and work
    if(baker_type == 3)
    {
      has_left_mitt = getLeftMitt( baker_type, baker_id, num_availible_left_mitts + 1,
                                   left_lock, mutex, left_availible );
      has_right_mitt = getRightMitt( baker_type, baker_id, num_availible_right_mitts + 1,
                                     right_lock, mutex, right_availible );

      if( has_left_mitt == 1 && has_right_mitt == 1 )
      {
        bake( baker_type, baker_id );
        has_left_mitt = returnLeftMitt( baker_type, baker_id, num_availible_left_mitts + 1,
                                        left_lock, mutex, left_availible );
        has_right_mitt = returnRightMitt( baker_type, baker_id, num_availible_right_mitts + 1,
                                          right_lock, mutex, right_availible );
      }
    }
  }
}

// funtion for bakers to work
void work( int type, int baker_id )
{
  if( type == 1 )
  {
    fprintf(stdout, "[Left-handed baker %d] is working...\n", baker_id);
  }
  else if( type == 2 )
  {
    fprintf(stdout, "[Right-handed baker %d] is working...\n", baker_id);
  }
  else
  {
    fprintf(stdout, "[Cautious baker %d] is working...\n", baker_id);
  }
  random_sleep( 0.2, 0.5 );
  return;
}

// function for bakers to get a right mitt
int getRightMitt( int type, int baker_id, int * num_availible_right_mitts,
                   pthread_mutex_t * right_lock, pthread_mutex_t * mutex, pthread_cond_t * right_availible )
{
  int ret_val = 0;

  // this is where the bakers complain about what they want
  if( type == 2 )
  {
    fprintf(stdout, "[Right-handed baker %d] wants a right-handed mitt...\n", baker_id);
  }
  else
  {
    fprintf(stdout, "[Cautious baker %d] wants a right-handed mitt...\n", baker_id);
  }

  // set lock for accessing the  number of availible right mitts
  pthread_mutex_lock( right_lock );

  // while there are no mitts to be gotten, tell the bakers to be patient until their
  // mitt is availible
  while( *num_availible_right_mitts <= 0 )
  {
    pthread_cond_wait( right_availible, right_lock );
  }

  // second lock for changing the number of availible right mitts
  pthread_mutex_lock( mutex );
  if( *num_availible_right_mitts > 0 )
  {
    // decrement the number of right mitts
    *num_availible_right_mitts-=1;
    ret_val = 1;

    // allow the baker to tell the world that they finally got thier precious mitt
    if( type == 2 )
    {
      fprintf(stdout, "[Right-handed baker %d] has got a right-handed mitt...\n", baker_id);
    }
    else if( type == 3)
    {
      fprintf(stdout, "[Cautious baker %d] has got a right-handed mitt...\n", baker_id);
    }

    // unlock inner lock
    pthread_mutex_unlock( mutex );
  }

  // unlock outer lock
  pthread_mutex_unlock( right_lock );

  // return value indicating that this baker has his requested mitt
  return ret_val;
}

// function for bakers to get a left mitt
int getLeftMitt( int type, int baker_id, int * num_availible_left_mitts,
                   pthread_mutex_t * left_lock, pthread_mutex_t * mutex, pthread_cond_t * left_availible  )
{
  int ret_val = 0;

  // bakers complaining about how they need a left mitt
  if( type == 1 )
  {
    fprintf(stdout, "[Left-handed baker %d] wants a left-handed mitt...\n", baker_id);
  }
  else
  {
    fprintf(stdout, "[Cautious baker %d] wants a left-handed mitt...\n", baker_id);
  }

  // lock the outer lock to access the number of availible left mitts
  pthread_mutex_lock( left_lock );

  // tell baker that they'll just have to wait if there are no left mitts
  // availible. We only have so much money to spend on mitts.
  while( *num_availible_left_mitts <= 0 )
  {
    pthread_cond_wait( left_availible, left_lock );
  }

  // lock the second lock to update the value of the number of left mitts and
  // let the baker grab one.
  pthread_mutex_lock( mutex );
  if( *num_availible_left_mitts > 0 )
  {
    // decrement the number of left mitts
    *num_availible_left_mitts-=1;
    ret_val = 1;

    // Reward the baker's patience with a chance to gloat to the other bakers
    // about how they have a mitt and the others are still waiting.
    if( type == 1 )
    {
      fprintf(stdout, "[Left-handed baker %d] has got a left-handed mitt...\n", baker_id);
    }
    else if( type == 3)
    {
      fprintf(stdout, "[Cautious baker %d] has got a left-handed mitt...\n", baker_id);
    }

    // unlock the inner lock
    pthread_mutex_unlock( mutex );
  }

  // unlock the outer lock
  pthread_mutex_unlock( left_lock );

  // return the value indicating that this baker has a left mitt
  return ret_val;
}

// function for bakers to return a right mitt
int returnRightMitt(int type, int baker_id, int * num_availible_right_mitts,
                pthread_mutex_t * right_lock, pthread_mutex_t * mutex, pthread_cond_t * right_availible )
{
  int ret_val = 1;

  // lock to update the number of mitts
  pthread_mutex_lock( right_lock );

  // add one mitt to the availible right mitts
  *num_availible_right_mitts+=1;
  ret_val = 0;

  // Give the baker the miracle of speech and let his coworkers know that a right
  // mitt has been returned
  if( type == 2 )
  {
    fprintf(stdout, "[Right-handed baker %d] has put back a right-handed mitt...\n", baker_id);
  }
  else if( type == 3)
  {
    fprintf(stdout, "[Cautious baker %d] has put back a right-handed mitt...\n", baker_id);
  }

  // signal the other bakers waiting for a right mitt that it's time to act like
  // it's black Friday and trample each other for a mitt
  pthread_cond_signal( right_availible );
  // unlock the lock
  pthread_mutex_unlock( right_lock );

  // return the value indicating that the baker no longer has a right mitt
  return ret_val;
}

// function for bakers to return a left mitt
int returnLeftMitt(int type, int baker_id, int * num_availible_left_mitts,
                pthread_mutex_t * left_lock, pthread_mutex_t * mutex, pthread_cond_t * left_availible )
{
  int ret_val = 1;

  // lock the lock to update the number of left mitts there are
  pthread_mutex_lock( left_lock );

  // put a mitt back
  *num_availible_left_mitts+=1;
  ret_val = 0;

  // Have the baker scream out that he is (finally) done using a left mitt
  if( type == 1 )
  {
    fprintf(stdout, "[Left-handed baker %d] has put back a left-handed mitt...\n", baker_id);
  }
  else if( type == 3)
  {
    fprintf(stdout, "[Cautious baker %d] has put back a left-handed mitt...\n", baker_id);
  }

  // signal the other bakers waiting and definitely not complaining that there
  // is now a left mitt availible to the first person who can grab it
  pthread_cond_signal( left_availible );
  // unlock the lock
  pthread_mutex_unlock( left_lock );

  // return the value indicating that the baker no longer has a left mitt
  return ret_val;
}

// function for bakers to bake
void bake(int type, int baker_id )
{
  // Tell the baker to speak and tell everyone he is now baking
  if( type == 1 )
  {
    fprintf(stdout, "[Left-handed baker %d] has put cookies in the oven and is waiting...\n", baker_id);
  }
  else if( type == 2 )
  {
    fprintf(stdout, "[Right-handed baker %d] has put cookies in the oven and is waiting...\n", baker_id);
  }
  else
  {
    fprintf(stdout, "[Cautious baker %d] has put cookies in the oven and is waiting...\n", baker_id);
  }

  // put him to sleep because putting virtual cookies in an oven is very taxing
  // on the human body
  random_sleep( 0.2, 0.5 );

  // Let the baker speak again and tell people he's done baking his cookies
  if( type == 1 )
  {
    fprintf(stdout, "[Left-handed baker %d] has taken cookies out of the oven...\n", baker_id);
  }
  else if( type == 2 )
  {
    fprintf(stdout, "[Right-handed baker %d] has taken cookies out of the oven...\n", baker_id);
  }
  else
  {
    fprintf(stdout, "[Cautious baker %d] has taken cookies out of the oven...\n", baker_id);
  }

  return;
}
