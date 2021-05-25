#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "util/str.h"

#define CONF_KV_LEN 64

struct conf {
    struct conf *next;

    char key[CONF_KV_LEN], value[CONF_KV_LEN];
};

static struct conf *config = NULL;

static int parse_conf(unsigned char *str, str_t *key, str_t *value)
{
    unsigned char *loop = str;

    while ((isspace(*loop) || !isprint(*loop)) && *loop != '\0')
        loop++;

    if (*loop == '#' || *loop == '\0')
        return -1;

    key->p = loop;
    while (*loop != '#' && *loop != '\0' && *loop != '=' && !isspace(*loop) &&
           isprint(*loop))
        loop++;

    key->len = loop - key->p;

    while (*loop != '#' && *loop != '\0' &&
           (*loop == '=' || isspace(*loop) || !isprint(*loop)))
        loop++;

    value->p = loop;
    while (*loop != '#' && *loop != '\0' && !isspace(*loop) && isprint(*loop))
        loop++;

    value->len = loop - value->p;

    return 0;
}

static void free_old_conf(struct conf *c)
{
    while (c) {
        struct conf *next = c->next;
        free(c);
        c = next;
    }
}

char *get_conf_entry(const char *key)
{
    for (struct conf *c = config; c; c = c->next) {
        if (str_equal(c->key, key))
            return c->value;
    }

    printf("Failed to find configuration item : %s\n", key);
    exit(0);
}

/* load config from file. All configurations will be proceeded into config */
void load_conf(const char *filename)
{
    char buff[512];
    struct conf *head = NULL;

    FILE *fp = fopen(filename, "r");
    if (NULL == fp) {
        printf("%s :%s\n", filename, strerror(errno));
        exit(0);
    }

    while (fgets(buff, sizeof(buff), fp)) {
        str_t key, value;
        if (parse_conf((unsigned char *) buff, &key, &value))
            continue;

        if (key.len >= CONF_KV_LEN || value.len >= CONF_KV_LEN) {
            printf("config item too long. [%.*s] [%.*s]\n", (int) key.len,
                   key.p, (int) value.len, value.p);
            fclose(fp);
            exit(0);
        }

        struct conf *c = malloc(sizeof(struct conf));
        if (!c) {
            printf("Failed to alloc mem for conf\n");
            fclose(fp);
            exit(0);
        }

        memcpy(c->key, key.p, key.len);
        c->key[key.len] = 0;
        memcpy(c->value, value.p, value.len);
        c->value[value.len] = 0;

        c->next = head ? head : NULL;
        head = c;
    }

    free_old_conf(config);
    config = head;
    fclose(fp);
}
