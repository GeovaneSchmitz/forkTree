#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "../fork_tree.h"

// Function to test process creation
int fork_test(fork_tree_t* tree, int argc, char* argv[]) {
    // This code can be replaced by any other code you want to generate the process tree
    for (int i = 0; i < 4; i++) {
        if (fork_tree_fork(tree) != 0) {
            for (int j = i; j < 4; j++) {
                if (fork_tree_fork(tree) != 0) {
                    break;
                }
            }
            break;
        }
    }
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

        // Renders the tree in the file
        if (fork_tree_render_centralized_svg(&fork_tree, file) == -1) {
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
