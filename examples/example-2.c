

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "../fork_tree.h"

// Function to test process creation
int fork_test(fork_tree_t* tree, int argc, char* argv[]) {
    // This code can be replaced by any other code you want to generate the process tree
    fork_tree_fork(tree);
    fork_tree_fork(tree);
    fork_tree_fork(tree);
    fork_tree_fork(tree);
    fork_tree_fork(tree);
    fork_tree_fork(tree);
    fork_tree_fork(tree);
    return 0;
}

int main(int argc, char* argv[]) {
    pid_t root_process_id = getpid();

    // Creates the structure that represents the process tree
    fork_tree_t fork_tree;
    // Initializes the structure
    fork_tree_init(&fork_tree);

    // Calls the function that creates the process tree
    int result = fork_test(&fork_tree, argc, argv);

    // Waits for all processes to finish
    while (wait(NULL) > 0)
        ;

    // Only the root process will render the tree
    if (getpid() == root_process_id) {
        // Opens the file to write the SVG
        FILE* file = fopen("example-1.svg", "w");

        /**
         * Renders the tree in the file
         * If you use fork_tree_render_centralized_svg instead of fork_tree_render_dense_svg,
         * the file width will be very, very large.
         * This is because the centralized version uses a lot of space to represent the tree.
         * The dense version, on the other hand, uses a lot less space, but it is not centralized.
         **/
        if (fork_tree_render_dense_svg(&fork_tree, file) == -1) {
            printf("Error while rendering tree\n");
            return -1;
        }

        // Closes the file
        fclose(file);

        // Destroys the structure
        fork_tree_destroy(&fork_tree);
    }

    return result;
}