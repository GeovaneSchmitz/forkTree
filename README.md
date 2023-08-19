## FORK TREE IMAGE GENERATOR

A utility for generating process fork tree images.

### Compiling

    To compile the example, run the following command:
    ```bash
        gcc examples/example-1.c fork_tree.c -o example-1
    ```

### Usage

You need to create a .c file with the code you want to see the fork tree.

In this file, you must include the fork_tree.h file and create a fork_tree_t. After that, you should replace all `fork()` calls with `fork_tree_fork(&fork_tree)`. Lastly, you should call the function `fork_tree_render_centralized_svg` or `fork_tree_render_dense_svg`

eg.

```c
#include "fork_tree.h"


int main(int argc, char* argv[]) {
    // Gets the process id of the root process
    pid_t root_process_id = getpid();

    // Creates the structure that represents the process tree
    fork_tree_t fork_tree;
    // Initializes the structure
    fork_tree_init(&fork_tree);

    /**
     * Your code here
     * eg. fork_tree_fork(&fork_tree);
     **/


    // Waits for all processes to finish
    while (wait(NULL) > 0)
        ;

    // Only the root process will render the tree
    if (getpid() == root_process_id) {
        // Opens the file to write the SVG
        FILE* file = fopen("output.svg", "w");

        /**
         * Renders the tree in the file
         * The first parameter is the structure that represents the tree
         * The second parameter is the file to write the SVG
         *
         * If the function returns -1, an error occurred
         * If the function returns 0, the tree was rendered successfully
         *
         * This function can be replaced by fork_tree_render_centralized_svg to render the tree in a centralized way
         *
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

    return 0;
}

```

### Examples

This is the code used to generate the image below, which can be found in the examples folder.

```c
/**
 *  Example 1
 **/
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
```

![Image](/assets/example.svg)

### Customizing

In the fork_tree.c file, you can change the following constants to customize the tree:

- CIRCLE_SIZE: The size of the circles
- CIRCLE_MARGIN_X: The horizontal margin between the circles
- CIRCLE_MARGIN_Y: The vertical margin between the circles
- DOCUMENT_MARGIN: The margin of the document

- BACKGROUND_COLOR: The background color of the document
- CIRCLE_COLOR: The color of the circles
- TEXT_COLOR: The color of the text (PID)
- LINE_COLOR: The color of the lines
- CONNECTOR_COLOR: The color of the connectors (circles that connect the lines)

- CIRCLE_SHADOW_COLOR: The color of the shadow of the circles
- CIRCLE_SHADOW_BLUR: The blur of the shadow of the circles
- CIRCLE_SHADOW_COLOR_OPACITY: The opacity of the shadow of the circles


Example:
```c

#define CIRCLE_SIZE 60
#define CIRCLE_MARGIN_X 20
#define CIRCLE_MARGIN_Y 40
#define DOCUMENT_MARGIN 20

#define BACKGROUND_COLOR "#162A29"
#define CIRCLE_COLOR "#FFFFFF"
#define LINE_COLOR "#22C84A"
#define TEXT_COLOR "#000000"
#define CONNECTOR_COLOR "#50FA7B"

#define CIRCLE_SHADOW_COLOR "#000000"
#define CIRCLE_SHADOW_BLUR "10"
#define CIRCLE_SHADOW_COLOR_OPACITY "0.3"
```

![Image](/assets/example-custom.svg)
