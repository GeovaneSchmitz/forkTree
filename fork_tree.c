#include "fork_tree.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CIRCLE_SIZE 60
#define CIRCLE_MARGIN_X 40
#define CIRCLE_MARGIN_Y 80
#define DOCUMENT_MARGIN 40

#define BACKGROUND_COLOR "#FFFFFF"
#define CIRCLE_COLOR "#000000"
#define LINE_COLOR "#000000"
#define TEXT_COLOR "#FFFFFF"
#define CONNECTOR_COLOR "#FF0000"

#define CIRCLE_SHADOW_COLOR "#000000"
#define CIRCLE_SHADOW_BLUR "5"
#define CIRCLE_SHADOW_COLOR_OPACITY "0.2"


int GLOBAL_COUNTER = 0;
// Number of nodes per page
// If the number of nodes is greater than the number of nodes per page, new pages are created.
#define NODE_PER_PAGE 20

typedef struct SharedTree {
    sem_t sem;
    pid_t root_process_id;
    int tree_id;
    int pages_fd;
    int number_of_pages;
} shared_tree_t;

typedef struct TreePage {
    int nodes[NODE_PER_PAGE];
    int parent[NODE_PER_PAGE];
} tree_page_t;

typedef struct MapNode {
    int key;
    struct MapNode *left;
    struct MapNode *right;
    void *value;
} map_node_t;

double get_width(map_node_t **width_map, map_node_t *child_map, int node, int is_dense);
char *fork_tree_gen_shared_tree_name_fd(int tree_number);
char *fork_tree_gen_page_name_fd(char *base_string);

void *map_get(map_node_t *root, int key) {
    if (root == NULL) {
        return NULL;
    }
    if (root->key == key) {
        return root->value;
    }
    if (root->key > key) {
        return map_get(root->left, key);
    }
    return map_get(root->right, key);
}

int map_put(map_node_t **root, int key, void *value) {
    if (*root == NULL) {
        *root = malloc(sizeof(map_node_t));
        if (*root == NULL)
            return -1;
        (*root)->key = key;
        (*root)->value = value;
        (*root)->left = NULL;
        (*root)->right = NULL;
        return 0;
    }
    if ((*root)->key == key) {
        (*root)->value = value;
        return 0;
    }
    if ((*root)->key > key) {
        return map_put(&((*root)->left), key, value);
    }
    return map_put(&((*root)->right), key, value);
}

int map_destroy(map_node_t *root) {
    if (root == NULL)
        return 0;
    map_destroy(root->left);
    map_destroy(root->right);
    free(root);
    return 0;
}

typedef struct LinkedListNode {
    void *value;
    struct LinkedListNode *next;
} linked_list_node_t;

typedef struct LinkedList {
    linked_list_node_t *head;
    linked_list_node_t *tail;
    int size;

} linked_list_t;

typedef struct canvasRegion {
    double min_x;
    double min_y;
    double max_x;
    double max_y;
} canvas_region_t;

int linked_list_add(linked_list_t *list, void *value) {
    linked_list_node_t *node = malloc(sizeof(linked_list_node_t));
    if (node == NULL)
        return -1;
    node->value = value;
    node->next = NULL;
    if (list->head == NULL) {
        list->head = node;
        list->tail = node;
        list->size = 1;
        return 0;
    }
    list->tail->next = node;
    list->tail = node;
    list->size++;
    return 0;
}

void linked_list_create(linked_list_t *list) {
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

void linked_list_destroy(linked_list_t *list) {
    linked_list_node_t *node = list->head;
    while (node != NULL) {
        linked_list_node_t *next = node->next;
        free(node);
        node = next;
    }
}

void *map_in_order(map_node_t *root, linked_list_t *list) {
    if (root == NULL)
        return NULL;
    map_in_order(root->left, list);

    linked_list_add(list, root->value);

    map_in_order(root->right, list);
}

/**
 * This function initializes a tree.
 * IT IS NOT THREAD SAFE.
 */
int fork_tree_init(fork_tree_t *tree) {
    memset(tree, 0, sizeof(fork_tree_t));

    char *tree_name = fork_tree_gen_shared_tree_name_fd(GLOBAL_COUNTER);
    if (tree_name == NULL) {
        return -1;
    }
    int fd = syscall(SYS_memfd_create, tree_name, 0);

    if (fd == -1) {
        return -1;
    }

    char *page_name = fork_tree_gen_page_name_fd(tree_name);
    if (page_name == NULL) {
        free(tree_name);
        return -1;
    }

    int pages_fd = syscall(SYS_memfd_create, page_name, 0);

    free(tree_name);
    free(page_name);

    if (pages_fd == -1) {
        return -1;
    }

    if (ftruncate(fd, sizeof(shared_tree_t)) == -1) {
        close(fd);
        return -1;
    }

    shared_tree_t *shared_tree = mmap(NULL, sizeof(shared_tree_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared_tree == MAP_FAILED) {
        return -1;
    }

    shared_tree->tree_id = GLOBAL_COUNTER;
    shared_tree->root_process_id = getpid();
    shared_tree->pages_fd = pages_fd;
    shared_tree->number_of_pages = 0;

    if (sem_init(&(shared_tree->sem), 1, 1) == -1) {
        munmap(shared_tree, sizeof(shared_tree_t));
        close(fd);
        return -1;
    }

    munmap(shared_tree, sizeof(shared_tree_t));

    tree->shared_tree_fd = fd;

    GLOBAL_COUNTER++;

    return 0;
}

char *fork_tree_gen_shared_tree_name_fd(int tree_number) {
    char *page_name = malloc(19 * sizeof(char));
    if (page_name == NULL) {
        return NULL;
    }

    if (tree_number >= 999999999) {
        free(page_name);
        return NULL;
    }

    if (sprintf(page_name, "p_tree_%d", tree_number) == 0) {
        free(page_name);
        return NULL;
    }

    return page_name;
}

char *fork_tree_gen_page_name_fd(char *base_string) {
    int len = strlen(base_string);
    char *page_name = malloc((len + 3) * sizeof(char));
    if (page_name == NULL) {
        return NULL;
    }
    strcpy(page_name, base_string);
    page_name[len] = '_';
    page_name[len + 1] = 'p';
    page_name[len + 2] = '\0';
    return page_name;
}

shared_tree_t *fork_tree_get_shared_tree(fork_tree_t *tree) {
    shared_tree_t *shared_tree = mmap(NULL, sizeof(shared_tree_t), PROT_READ | PROT_WRITE, MAP_SHARED, tree->shared_tree_fd, 0);
    if (shared_tree == MAP_FAILED) {
        return NULL;
    }
    return shared_tree;
}

int fork_tree_add_node(fork_tree_t *tree, pid_t parent_pid, pid_t child_pid) {
    shared_tree_t *shared_tree = fork_tree_get_shared_tree(tree);

    if (shared_tree == NULL) {
        printf("Error getting shared tree\n");
        return -1;
    }

    sem_wait(&shared_tree->sem);

    if (shared_tree->number_of_pages == 0) {
        if (ftruncate(shared_tree->pages_fd, sizeof(tree_page_t)) == -1) {
            printf("Error truncating pages file\n");
            sem_post(&shared_tree->sem);
            return -1;
        }
        shared_tree->number_of_pages++;
    }

    tree_page_t *pages = mmap(
        NULL,
        sizeof(tree_page_t) * shared_tree->number_of_pages,
        PROT_READ | PROT_WRITE, MAP_SHARED,
        shared_tree->pages_fd, 0);

    if (pages == MAP_FAILED) {
        sem_post(&shared_tree->sem);
        int errsv = errno;
        printf("Error:%d\n", errsv);
        return -1;
    }

    int inserted = 0;

    tree_page_t *last_page = &pages[shared_tree->number_of_pages - 1];

    for (int i = 0; i < NODE_PER_PAGE; i++) {
        if (last_page->nodes[i] == 0) {
            last_page->nodes[i] = child_pid;
            last_page->parent[i] = parent_pid;
            inserted = 1;
            msync(last_page, sizeof(tree_page_t) * shared_tree->number_of_pages, MS_SYNC);
            break;
        }
    }

    munmap(pages, sizeof(tree_page_t) * shared_tree->number_of_pages);

    if (!inserted) {
        shared_tree->number_of_pages++;
        if (ftruncate(shared_tree->pages_fd, sizeof(tree_page_t) * shared_tree->number_of_pages) == -1) {
            printf("Error truncating pages file\n");
            sem_post(&shared_tree->sem);
            return -1;
        }

        tree_page_t *pages = mmap(
            NULL,
            sizeof(tree_page_t) * shared_tree->number_of_pages,
            PROT_READ | PROT_WRITE, MAP_SHARED,
            shared_tree->pages_fd,
            0);

        if (pages == MAP_FAILED) {
            printf("Error mapping last page\n");
            sem_post(&shared_tree->sem);
            return -1;
        }
        tree_page_t *last_page = &pages[shared_tree->number_of_pages - 1];

        last_page->nodes[0] = child_pid;
        last_page->parent[0] = parent_pid;

        msync(pages, sizeof(tree_page_t) * shared_tree->number_of_pages, MS_SYNC);
        munmap(pages, sizeof(tree_page_t) * shared_tree->number_of_pages);
    }

    sem_post(&shared_tree->sem);
    return 0;
}

int fork_tree_fork(fork_tree_t *tree) {
    int forked = fork();
    if (forked != 0) {
        fork_tree_add_node(tree, getpid(), forked);
    }
    return forked;
}

int create_circle(FILE *fd, int node, double cx, double cy) {
    int total = 0;
    int result = fprintf(fd, "<circle cx=\"%g\" cy=\"%g\" r=\"%g\" fill=\"" CIRCLE_SHADOW_COLOR "\" opacity=\"" CIRCLE_SHADOW_COLOR_OPACITY "\" filter=\"url(#igs-shadow)\"></circle>", cx, cy, (double)(CIRCLE_SIZE) / 2);
    if (result < 0) {
        printf("Error writing to file\n");
        return result;
    }
    total += result;

    result = fprintf(fd, "<circle cx=\"%g\" cy=\"%g\" r=\"%g\" fill=\"" CIRCLE_COLOR "\"></circle>", cx, cy, (double)(CIRCLE_SIZE) / 2);
    if (result < 0) {
        printf("Error writing to file\n");
        return result;
    }
    total += result;

    result = fprintf(fd, "<text font-family=\"-apple-system,system-ui,BlinkMacSystemFont,'Segoe UI',Roboto,'Helvetica Neue',Arial,sans-serif\" x=\"%g\" y=\"%g\" text-anchor=\"middle\" dominant-baseline=\"middle\" fill=\"" TEXT_COLOR "\">%d</text>", cx, cy, node);
    if (result < 0) {
        printf("Error writing to file\n");
        return result;
    }
    total += result;
    return total;
}

int create_line(FILE *fd, double parent_x, double parent_y, double child_x, double child_y, int is_last_child) {
    double mid_y = (parent_y + child_y) / 2;
    double start_y = parent_y + (double)(CIRCLE_SIZE) / 2;
    double end_y = child_y - (double)(CIRCLE_SIZE) / 2;
    int total = 0;
    int result = fprintf(fd, "<path d=\"M%g,%g C%g,%g %g,%g %g,%g\" stroke=\"" LINE_COLOR "\" stroke-width=\"2\" fill=\"none\"></path>", parent_x, start_y, parent_x, mid_y, child_x, mid_y, child_x, end_y);
    if (result < 0) {
        printf("Error writing line\n");
        return result;
    }
    total += result;

    result = fprintf(fd, "<circle cx=\"%g\" cy=\"%g\" r=\"5\" fill=\"" CONNECTOR_COLOR "\"></circle>", child_x, end_y);
    if (result < 0) {
        printf("Error writing line\n");
        return result;
    }
    total += result;

    if (is_last_child) {
        result = fprintf(fd, "<circle cx=\"%g\" cy=\"%g\" r=\"5\" fill=\"" CONNECTOR_COLOR "\"></circle>", parent_x, start_y);
        if (result < 0) {
            printf("Error writing line\n");
            return result;
        }
    }
    total += result;

    return total;
}

double get_width(map_node_t **width_map, map_node_t *child_map, int node, int is_dense) {
    if (map_get(*width_map, node) != NULL) {
        double *size = map_get(*width_map, node);
        return *size;
    }

    linked_list_t *children = map_get(child_map, node);
    if (children == NULL) {
        return (double)(CIRCLE_SIZE);
    }

    double max_size = 0;
    double total_size = 0;
    linked_list_node_t *current = children->head;
    while (current != NULL) {
        int *child = current->value;
        double size = get_width(width_map, child_map, *child, is_dense);
        if (size > max_size) {
            max_size = size;
        }
        total_size += size;
        current = current->next;
    }
    double *size = malloc(sizeof(double));
    if (size == NULL) {
        printf("Error allocating size\n");
        return -1;
    }

    if (is_dense) {
        *size = total_size + (double)(CIRCLE_MARGIN_X) * (children->size - 1);
    } else {
        *size = max_size * children->size + (double)(CIRCLE_MARGIN_X) * (children->size - 1);
    }
    if (map_put(width_map, node, size)) {
        printf("Error putting in width map\n");
        return -1;
    }
    return *size;
}

int render_tree(FILE *fd, map_node_t **width_map, map_node_t *child_map, canvas_region_t *canvas_region, int node, int level, double base_x, int is_line, int is_dense) {
    linked_list_t *children = map_get(child_map, node);
    if (children == NULL) {
        return 0;
    }

    double size = get_width(width_map, child_map, node, is_dense);

    double step = 0;
    if (!is_dense) {
        step = size / children->size;
    }

    double offset_x = base_x - size / 2 - step / 2;

    int is_root = level == 1;
    double parent_y = (double)(CIRCLE_SIZE) / 2 + ((double)(CIRCLE_SIZE) + (double)(CIRCLE_MARGIN_Y)) * (level - 1);
    if (!is_line && is_root) {
        int result = create_circle(fd, node, base_x, parent_y);
        if (result < 0) {
            printf("Error creating circle\n");
            return result;
        }

        double half_circle_size = (double)(CIRCLE_SIZE) / 2;

        double min_x = base_x - half_circle_size;
        double max_x = base_x + half_circle_size;

        if (canvas_region->min_x > min_x) {
            canvas_region->min_x = min_x;
        }

        if (canvas_region->max_x < max_x) {
            canvas_region->max_x = max_x;
        }

        double min_y = parent_y - half_circle_size;
        double max_y = parent_y + half_circle_size;

        if (canvas_region->min_y > min_y) {
            canvas_region->min_y = min_y;
        }

        if (canvas_region->max_y < max_y) {
            canvas_region->max_y = max_y;
        }
    }

    double y = (double)(CIRCLE_SIZE) / 2 + ((double)(CIRCLE_SIZE) + (double)(CIRCLE_MARGIN_Y)) * level;
    linked_list_node_t *current = children->head;
    int i = 0;
    while (current != NULL) {
        int *child = current->value;
        double child_size = get_width(width_map, child_map, *child, is_dense);

        double x;
        if (is_dense) {
            x = offset_x + child_size / 2;
            offset_x += child_size + (double)(CIRCLE_MARGIN_X);
        } else {
            x = offset_x + step * (i + 1);
        }

        if (is_line) {
            int result = create_line(fd, base_x, parent_y, x, y, current->next == NULL);
            if (result < 0) {
                printf("Error creating line\n");
                return result;
            }
        } else {
            int result = create_circle(fd, *child, x, y);
            if (result < 0) {
                printf("Error creating circle\n");
                return result;
            }

            double min_x = x - (double)(CIRCLE_SIZE) / 2;
            double max_x = x + (double)(CIRCLE_SIZE) / 2;

            if (canvas_region->min_x > min_x) {
                canvas_region->min_x = min_x;
            }

            if (canvas_region->max_x < max_x) {
                canvas_region->max_x = max_x;
            }

            double min_y = y - (double)(CIRCLE_SIZE) / 2;
            double max_y = y + (double)(CIRCLE_SIZE) / 2;

            if (canvas_region->min_y > min_y) {
                canvas_region->min_y = min_y;
            }

            if (canvas_region->max_y < max_y) {
                canvas_region->max_y = max_y;
            }
        }
        if (render_tree(fd, width_map, child_map, canvas_region, *child, level + 1, x, is_line, is_dense) == -1) {
            return -1;
        }
        i++;
        current = current->next;
    }
    return 0;
}

void width_map_destroy(map_node_t *width_map) {
    free(width_map->value);
    if (width_map->left != NULL) {
        width_map_destroy(width_map->left);
    }
    if (width_map->right != NULL) {
        width_map_destroy(width_map->right);
    }
}

void child_map_destroy(map_node_t *child_map) {
    linked_list_t *children = child_map->value;
    linked_list_node_t *current = children->head;
    while (current != NULL) {
        free(current->value);
        current = current->next;
    }
    linked_list_destroy(children);
    free(children);
    if (child_map->left != NULL) {
        child_map_destroy(child_map->left);
    }
    if (child_map->right != NULL) {
        child_map_destroy(child_map->right);
    }
}

void fork_tree_render_cleanup(shared_tree_t *shared_tree, map_node_t *child_map, map_node_t *width_map, FILE *fd) {
    width_map_destroy(width_map);
    map_destroy(width_map);
    child_map_destroy(child_map);
    map_destroy(child_map);
    fclose(fd);
    sem_post(&shared_tree->sem);
}

int fork_tree_render_centralized_svg(fork_tree_t *tree, FILE *fd) {
    shared_tree_t *shared_tree = fork_tree_get_shared_tree(tree);

    if (shared_tree == NULL) {
        return -1;
    }

    sem_wait(&shared_tree->sem);
    if (shared_tree->number_of_pages == 0) {
        return 0;
    }

    tree_page_t *pages = mmap(NULL, sizeof(tree_page_t) * shared_tree->number_of_pages, PROT_READ, MAP_PRIVATE, shared_tree->pages_fd, 0);

    if (pages == MAP_FAILED) {
        printf("Error mapping pages\n");
        sem_post(&shared_tree->sem);
        return -1;
    }

    map_node_t *map = NULL;
    map_node_t *width_map = NULL;
    FILE *tmp = tmpfile();

    for (int i = 0; i < shared_tree->number_of_pages; i++) {
        tree_page_t *current_page = &pages[i];
        for (int j = 0; j < NODE_PER_PAGE; j++) {
            if (current_page->nodes[j] != 0) {
                linked_list_t *list = map_get(map, current_page->parent[j]);
                if (list == NULL) {
                    list = malloc(sizeof(linked_list_t));
                    if (list == NULL) {
                        printf("Error allocating memory\n");
                        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
                        return -1;
                    }
                    linked_list_create(list);
                    if (map_put(&map, current_page->parent[j], list) == -1) {
                        printf("Error putting in map\n");
                        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
                        return -1;
                    }
                }
                int *value = malloc(sizeof(int));
                if (value == NULL) {
                    printf("Error allocating memory\n");
                    fork_tree_render_cleanup(shared_tree, map, width_map, tmp);

                    return -1;
                }
                *value = current_page->nodes[j];
                if (linked_list_add(list, value) == -1) {
                    printf("Error adding to list\n");
                    fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
                    return -1;
                }
            }
        }
    }
    get_width(&width_map, map, shared_tree->root_process_id, 0);

    canvas_region_t canvas_region = {
        .max_x = -INFINITY,
        .max_y = -INFINITY,
        .min_x = INFINITY,
        .min_y = INFINITY};

    if (render_tree(tmp, &width_map, map, &canvas_region, shared_tree->root_process_id, 1, 0, 1, 0) == -1) {
        printf("Error rendering tree lines\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    if (render_tree(tmp, &width_map, map, &canvas_region, shared_tree->root_process_id, 1, 0, 0, 0) == -1) {
        printf("Error rendering tree circle\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    canvas_region.max_x += DOCUMENT_MARGIN;
    canvas_region.max_y += DOCUMENT_MARGIN;
    canvas_region.min_x -= DOCUMENT_MARGIN;
    canvas_region.min_y -= DOCUMENT_MARGIN;

    int result = fprintf(fd, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    if (result < 0) {
        printf("Error writing xml tag to file\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    double canvas_width = canvas_region.max_x - canvas_region.min_x;
    double canvas_height = canvas_region.max_y - canvas_region.min_y;

    result = fprintf(fd, "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" viewBox=\"%g %g %g %g\">", canvas_region.min_x, canvas_region.min_y, canvas_width, canvas_height);
    if (result < 0) {
        printf("Error writing svg tag to file\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    result = fprintf(fd, "<defs><filter id=\"igs-shadow\"><feGaussianBlur in=\"SourceGraphic\" stdDeviation=\"" CIRCLE_SHADOW_BLUR "\"></feGaussianBlur></filter></defs>");
    if (result < 0) {
        printf("Error writing def tag to file\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    result = fprintf(fd, "<rect x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\" fill=\"" BACKGROUND_COLOR "\"/>", canvas_region.min_x, canvas_region.min_y, canvas_width, canvas_height);
    if (result < 0) {
        printf("Error writing background tag to file\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    int end_position = ftell(tmp);

    if (end_position == -1) {
        printf("Error getting position\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    if (fseek(tmp, 0, SEEK_SET) == -1) {
        printf("Error seeking\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    for (int i = 0; i < end_position; i++) {
        int c = fgetc(tmp);
        if (c == EOF) {
            printf("Error reading tmp file\n");
            fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
            return -1;
        }
        result = fputc(c, fd);
        if (result == EOF) {
            printf("Error writing to file\n");
            fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
            return -1;
        }
    }

    result = fprintf(fd, "</svg>");
    if (result < 0) {
        printf("Error writing svg tag to file\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    // document_margin

    canvas_region.min_x -= DOCUMENT_MARGIN;
    canvas_region.min_y -= DOCUMENT_MARGIN;
    canvas_region.max_x += DOCUMENT_MARGIN;
    canvas_region.max_y += DOCUMENT_MARGIN;

    munmap(pages, sizeof(tree_page_t) * shared_tree->number_of_pages);
    fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
    return 0;
}

int fork_tree_render_dense_svg(fork_tree_t *tree, FILE *fd) {
    shared_tree_t *shared_tree = fork_tree_get_shared_tree(tree);

    if (shared_tree == NULL) {
        return -1;
    }

    sem_wait(&shared_tree->sem);
    if (shared_tree->number_of_pages == 0) {
        return 0;
    }

    tree_page_t *pages = mmap(NULL, sizeof(tree_page_t) * shared_tree->number_of_pages, PROT_READ, MAP_PRIVATE, shared_tree->pages_fd, 0);

    if (pages == MAP_FAILED) {
        printf("Error mapping pages\n");
        sem_post(&shared_tree->sem);
        return -1;
    }

    map_node_t *map = NULL;
    map_node_t *width_map = NULL;
    FILE *tmp = tmpfile();

    for (int i = 0; i < shared_tree->number_of_pages; i++) {
        tree_page_t *current_page = &pages[i];
        for (int j = 0; j < NODE_PER_PAGE; j++) {
            if (current_page->nodes[j] != 0) {
                linked_list_t *list = map_get(map, current_page->parent[j]);
                if (list == NULL) {
                    list = malloc(sizeof(linked_list_t));
                    if (list == NULL) {
                        printf("Error allocating memory\n");
                        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
                        return -1;
                    }
                    linked_list_create(list);
                    if (map_put(&map, current_page->parent[j], list) == -1) {
                        printf("Error putting in map\n");
                        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
                        return -1;
                    }
                }
                int *value = malloc(sizeof(int));
                if (value == NULL) {
                    printf("Error allocating memory\n");
                    fork_tree_render_cleanup(shared_tree, map, width_map, tmp);

                    return -1;
                }
                *value = current_page->nodes[j];
                if (linked_list_add(list, value) == -1) {
                    printf("Error adding to list\n");
                    fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
                    return -1;
                }
            }
        }
    }
    get_width(&width_map, map, shared_tree->root_process_id, 1);

    canvas_region_t canvas_region = {
        .max_x = -INFINITY,
        .max_y = -INFINITY,
        .min_x = INFINITY,
        .min_y = INFINITY};

    if (render_tree(tmp, &width_map, map, &canvas_region, shared_tree->root_process_id, 1, 0, 1, 1) == -1) {
        printf("Error rendering tree lines\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    if (render_tree(tmp, &width_map, map, &canvas_region, shared_tree->root_process_id, 1, 0, 0, 1) == -1) {
        printf("Error rendering tree circle\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    canvas_region.max_x += DOCUMENT_MARGIN;
    canvas_region.max_y += DOCUMENT_MARGIN;
    canvas_region.min_x -= DOCUMENT_MARGIN;
    canvas_region.min_y -= DOCUMENT_MARGIN;

    int result = fprintf(fd, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    if (result < 0) {
        printf("Error writing xml tag to file\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    double canvas_width = canvas_region.max_x - canvas_region.min_x;
    double canvas_height = canvas_region.max_y - canvas_region.min_y;

    result = fprintf(fd, "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" viewBox=\"%g %g %g %g\">", canvas_region.min_x, canvas_region.min_y, canvas_width, canvas_height);
    if (result < 0) {
        printf("Error writing svg tag to file\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    result = fprintf(fd, "<defs><filter id=\"igs-shadow\"><feGaussianBlur in=\"SourceGraphic\" stdDeviation=\"" CIRCLE_SHADOW_BLUR "\"></feGaussianBlur></filter></defs>");
    if (result < 0) {
        printf("Error writing def tag to file\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    result = fprintf(fd, "<rect x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\" fill=\"" BACKGROUND_COLOR "\"/>", canvas_region.min_x, canvas_region.min_y, canvas_width, canvas_height);
    if (result < 0) {
        printf("Error writing background tag to file\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    int end_position = ftell(tmp);

    if (end_position == -1) {
        printf("Error getting position\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    if (fseek(tmp, 0, SEEK_SET) == -1) {
        printf("Error seeking\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    for (int i = 0; i < end_position; i++) {
        int c = fgetc(tmp);
        if (c == EOF) {
            printf("Error reading tmp file\n");
            fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
            return -1;
        }
        result = fputc(c, fd);
        if (result == EOF) {
            printf("Error writing to file\n");
            fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
            return -1;
        }
    }

    result = fprintf(fd, "</svg>");
    if (result < 0) {
        printf("Error writing svg tag to file\n");
        fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
        return -1;
    }

    // document_margin

    canvas_region.min_x -= DOCUMENT_MARGIN;
    canvas_region.min_y -= DOCUMENT_MARGIN;
    canvas_region.max_x += DOCUMENT_MARGIN;
    canvas_region.max_y += DOCUMENT_MARGIN;

    munmap(pages, sizeof(tree_page_t) * shared_tree->number_of_pages);
    fork_tree_render_cleanup(shared_tree, map, width_map, tmp);
    return 0;
}

void fork_tree_destroy(fork_tree_t *tree) {
    shared_tree_t *shared_tree = fork_tree_get_shared_tree(tree);
    if (shared_tree == NULL) {
        return;
    }

    sem_wait(&shared_tree->sem);
    close(shared_tree->pages_fd);

    sem_destroy(&shared_tree->sem);
    munmap(shared_tree, sizeof(shared_tree_t));
    close(tree->shared_tree_fd);
}