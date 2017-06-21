/*
 * choice.c - this cleri element can hold other elements and the grammar
 *            has to choose one of them.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *  - fixed issue #53, 23-02-2017
 */
#include <cleri/choice.h>
#include <cleri/node.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

static void CHOICE_free(cleri_object_t * cl_object);

static cleri_node_t * CHOICE_parse(
        cleri_parse_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

static cleri_node_t * CHOICE_parse_most_greedy(
        cleri_parse_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

static cleri_node_t * CHOICE_parse_first_match(
        cleri_parse_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule);

/*
 * Returns NULL in case an error has occurred.
 */
cleri_object_t * cleri_choice(
        uint32_t gid,
        int most_greedy,
        size_t len,
        ...)
{
    va_list ap;

    cleri_object_t * cl_object = cleri_object_new(
            gid,
            CLERI_TP_CHOICE,
            &CHOICE_free,
            &CHOICE_parse);

    if (cl_object == NULL)
    {
        return NULL;
    }

    cl_object->via.choice =
            (cleri_choice_t *) malloc(sizeof(cleri_choice_t));

    if (cl_object->via.choice == NULL)
    {
        free(cl_object);
        return NULL;
    }

    cl_object->via.choice->most_greedy = most_greedy;
    cl_object->via.choice->olist = cleri__olist_new();

    if (cl_object->via.choice->olist == NULL)
    {
        cleri_object_free(cl_object);
        return NULL;
    }

    va_start(ap, len);
    while(len--)
    {
        if (cleri__olist_append(
                cl_object->via.choice->olist,
                va_arg(ap, cleri_object_t *)))
        {
            cleri__olist_cancel(cl_object->via.choice->olist);
            cleri_object_free(cl_object);
            cl_object = NULL;
            break;
        }
    }
    va_end(ap);

    return cl_object;
}

/*
 * Destroy choice object.
 */
static void CHOICE_free(cleri_object_t * cl_object)
{
    cleri__olist_free(cl_object->via.choice->olist);
    free(cl_object->via.choice);
}

/*
 * Returns a node or NULL.
 */
static cleri_node_t * CHOICE_parse(
        cleri_parse_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    return (cl_obj->via.choice->most_greedy) ?
            CHOICE_parse_most_greedy(pr, parent, cl_obj, rule) :
            CHOICE_parse_first_match(pr, parent, cl_obj, rule);
}

/*
 * Returns a node or NULL. In case of an error pr->is_valid is set to -1.
 */
static cleri_node_t * CHOICE_parse_most_greedy(
        cleri_parse_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_olist_t * olist;
    cleri_node_t * node;
    cleri_node_t * rnode;
    cleri_node_t * mg_node = NULL;
    const char * str = parent->str + parent->len;

    olist = cl_obj->via.choice->olist;
    while (olist != NULL)
    {
        if ((node = cleri__node_new(cl_obj, str, 0)) == NULL)
        {
            pr->is_valid = -1;
            return NULL;
        }
        rnode = cleri__parse_walk(
                pr,
                node,
                olist->cl_obj,
                rule,
                CLERI__EXP_MODE_REQUIRED);
        if (rnode != NULL && (mg_node == NULL || node->len > mg_node->len))
        {
            cleri__node_free(mg_node);
            mg_node = node;
        }
        else
        {
            cleri__node_free(node);
        }
        olist = olist->next;
    }
    if (mg_node != NULL)
    {
        parent->len += mg_node->len;
        if (cleri__children_add(parent->children, mg_node))
        {
             /* error occurred, reverse changes set mg_node to NULL */
            pr->is_valid = -1;
            parent->len -= mg_node->len;
            cleri__node_free(mg_node);
            mg_node = NULL;
        }
    }
    return mg_node;
}

/*
 * Returns a node or NULL. In case of an error pr->is_valid is set to -1.
 */
static cleri_node_t * CHOICE_parse_first_match(
        cleri_parse_t * pr,
        cleri_node_t * parent,
        cleri_object_t * cl_obj,
        cleri_rule_store_t * rule)
{
    cleri_olist_t * olist;
    cleri_node_t * node;
    cleri_node_t * rnode;

    olist = cl_obj->via.choice->olist;
    node = cleri__node_new(cl_obj, parent->str + parent->len, 0);
    if (node == NULL)
    {
        pr->is_valid = -1;
        return NULL;
    }
    while (olist != NULL)
    {
        rnode = cleri__parse_walk(
                pr,
                node,
                olist->cl_obj,
                rule,
                CLERI__EXP_MODE_REQUIRED);
        if (rnode != NULL)
        {
            parent->len += node->len;
            if (cleri__children_add(parent->children, node))
            {
                 /* error occurred, reverse changes set mg_node to NULL */
                pr->is_valid = -1;
                parent->len -= node->len;
                cleri__node_free(node);
                node = NULL;
            }
            return node;
        }
        olist = olist->next;
    }
    cleri__node_free(node);
    return NULL;
}
