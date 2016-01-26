/*
 * web handler
 *
 * Author: junwei.dong
 */
#include "common.h"

int __ch_isspace(char c)
{
    return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}
void _strim(char *str)
{
    char *p1, *p2, c;
    size_t len;

    if((len = strlen(str)) == 0)
        return;

    /* Determine start position. */
    for(p1 = str; (c = *p1); p1++)
    {
        if(!__ch_isspace(c))
            break;
    }

    /* Determine ending position. */
    for(p2 = str + len; p2 > p1 && (c = *(p2 - 1)); p2--)
    {
        if(!__ch_isspace(c))
            break;
    }

    /* Move string ahead, and put new terminal character. */
    memmove(str, p1, (size_t)(p2 - p1));
    str[(size_t)(p2 - p1)] = '\0';
}

int strcmp_nocase(char * src, char * mark)
{
    int i;
    int ret = 0;
    char s;
    char m;

    if (src == NULL || mark == NULL)
    {
        return -1;
    }

    for (i = 0;;i++)
    {
        s = tolower(*(src + i));
        m = tolower(*(mark + i));
        if (s > m)
        {
            ret = i + 1;
            break;
        }
        else if (s < m)
        {
            ret = 0 - i - 1;
            break;
        }

        if (s == 0 || m == 0)
        {
            break;
        }
    }
    return ret;
}



/****************************************************************
 *
 *  Function: mSplit()
 *
 *  Purpose: Splits a string into tokens non-destructively.
 *
 *  Parameters:
 *      char *str => the string to be split
 *      char *sep => a string of token seperaters
 *      int max_strs => how many tokens should be returned
 *      int *toks => place to store the number of tokens found in str
 *      char meta => the "escape metacharacter", treat the character
 *                   after this character as a literal and "escape" a
 *                   seperator
 *
 *  Returns:
 *      2D char array with one token per "row" of the returned
 *      array.
 *
 ****************************************************************/
char **mSplit(char *str, char *sep, int max_strs, int *toks, char meta)
{
    char **retstr;      /* 2D array which is returned to caller */
    char *idx;          /* index pointer into str */
    char *end;          /* ptr to end of str */
    char *sep_end;      /* ptr to end of seperator string */
    char *sep_idx;      /* index ptr into seperator string */
    int len = 0;        /* length of current token string */
    int curr_str = 0;       /* current index into the 2D return array */
    char last_char = (char) 0xFF;

    if(!toks) return NULL;

    *toks = 0;

    if (!str) return NULL;
    /*
     * find the ends of the respective passed strings so our while() loops
     * know where to stop
     */
    sep_end = sep + strlen(sep);
    end = str + strlen(str);

    /* remove trailing whitespace */
    while(isspace((int) *(end - 1)) && ((end - 1) >= str))
        *(--end) = '\0';    /* -1 because of NULL */

    /* set our indexing pointers */
    sep_idx = sep;
    idx = str;

    /*
     * alloc space for the return string, this is where the pointers to the
     * tokens will be stored
     */
    if((retstr = (char **) malloc((sizeof(char **) * max_strs))) == NULL) {
        //D("malloc");
        marbit_send_log(ERROR,"malloc");
    }

    max_strs--;
    /* loop thru each letter in the string being tokenized */
    while(idx < end)
    {
        /* loop thru each seperator string char */
        while(sep_idx < sep_end)
        {
            /*
             * if the current string-indexed char matches the current
             * seperator char...
             */
            if((*idx == *sep_idx) && (last_char != meta))
            {
                /* if there's something to store... */
                if(len > 0)
                {
                    if(curr_str <= max_strs)
                    {
                        /* allocate space for the new token */
                        if((retstr[curr_str] = (char *)malloc((sizeof(char) * len) + 1)) == NULL)
                        {
                            marbit_send_log(ERROR,"malloc");
                        }

                        /* copy the token into the return string array */
                        memcpy(retstr[curr_str], (idx - len), len);
                        retstr[curr_str][len] = 0;
                        /* twiddle the necessary pointers and vars */
                        len = 0;
                        curr_str++;
                        last_char = *idx;
                        idx++;
                    }
                   /*
                     * if we've gotten all the tokens requested, return the
                     * list
                     */
                    if(curr_str >= max_strs)
                    {
                        while(isspace((int) *idx))
                            idx++;

                        len = end - idx;
                        fflush(stdout);

                        if((retstr[curr_str] = (char *)malloc((sizeof(char) * len) + 1)) == NULL) 
                        {
                            marbit_send_log(ERROR,"malloc");
                        }
                        memcpy(retstr[curr_str], idx, len);
                        retstr[curr_str][len] = 0;

                        *toks = curr_str + 1;
                        return retstr;
                    }
                }
                else
                    /*
                     * otherwise, the previous char was a seperator as well,
                     * and we should just continue
                     */
                {
                    last_char = *idx;
                    idx++;
                    /* make sure to reset this so we test all the sep. chars */
                    sep_idx = sep;
                    len = 0;
                }
            }
            else
            {
                /* go to the next seperator */
                sep_idx++;
            }
        }

        sep_idx = sep;
        len++;
        last_char = *idx;
        idx++;
    }

    /* put the last string into the list */

    if(len > 0)
    {
        if((retstr[curr_str] = (char *)
                    malloc((sizeof(char) * len) + 1)) == NULL) 
        {
            marbit_send_log(ERROR,"malloc");
        }
        memcpy(retstr[curr_str], (idx - len), len);
        retstr[curr_str][len] = 0;

        *toks = curr_str + 1;
    }

    /* return the token list */
    return retstr;
}




/****************************************************************
 *
 * Free the buffer allocated by mSplit().
 *
 * char** toks = NULL;
 * int num_toks = 0;
 * toks = (str, " ", 2, &num_toks, 0);
 * mSplitFree(&toks, num_toks);
 *
 * At this point, toks is again NULL.
 *
 ****************************************************************/
void mSplitFree(char ***pbuf, int num_toks)
{
    int i;
    char** buf;  /* array of string pointers */

    if( pbuf==NULL || *pbuf==NULL )
    {
        return;
    }

    buf = *pbuf;

    for( i=0; i<num_toks; i++ )
    {
        if( buf[i] != NULL )
        {
            free( buf[i] );
            buf[i] = NULL;
        }
    }

    free(buf);
    *pbuf = NULL;
}

