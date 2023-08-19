
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct ForkTree {
    int shared_tree_fd;
} fork_tree_t;

/** 
 * Initialize a fork tree
 * 
 * THIS FUNCTION IS NOT THREAD SAFE
 */
int fork_tree_init(fork_tree_t *tree);

/* Fork a new process and add it to the tree */
int fork_tree_fork(fork_tree_t *tree);

/**
 * Render the tree to a file in SVG format.
 * This function renders the tree in a centralized way, where the children are evenly distributed.
 * 
 **/
int fork_tree_render_centralized_svg(fork_tree_t *tree, FILE *file);

/**
 * Render the tree to a file in SVG format.
 * This function renders the tree in a dense way all node in the smallest possible space. 
*/
int fork_tree_render_dense_svg(fork_tree_t *tree, FILE *file);


// Destroy the fork tree
void fork_tree_destroy(fork_tree_t *tree);