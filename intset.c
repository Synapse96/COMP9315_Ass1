#include "postgres.h"
#include "fmgr.h"

PG_MODULE_MAGIC;

typedef struct Intset
{
	char[4]	v1_len;
	char[1]	data;
	int cdn; //cardinality?
} Intset;

Intset *parse_set(char *str) {
  int *arr = (int*)malloc(strlen(str)*sizeof(int));
  int j = 0, temp = 0, isNeg = 0;
  int numFound = 0;
  for(int i = 1; i < strlen(str); i++) {
    if (str[i] == ' ') {
      if (numFound == 1) {
        numFound = 2;
      } 
      continue;
    } else if (str[i] == ',' || str[i] == '}') {
      if (numFound != 0) {
        numFound = 0;
        arr[j] = temp; 
        temp = 0;
        isNeg = 0;
        j++;
      } else {
        return NULL;
      }
    } else if (str[i] >= '0' && str[i] <= '9') {
      if (numFound == 0) {
        numFound = 1;
      } else if (numFound == 2) {
        return NULL;
      }
      if (!isNeg) {
        temp = 10*temp + (str[i] - '0');
      } else {
        temp = 10*temp - (str[i] - '0');
      }
    } else if (str[i] == '-') {
      isNeg = 1;
    } 
  }
  Intset *s = (Intset *)palloc(sizeof(Intset));
  SET_VARSIZE(s,j);
  memcpy(s->data,arr,j);
  return s;
}

/*****************************************************************************
 * Input/Output functions
 *****************************************************************************/

PG_FUNCTION_INFO_V1(intset_in);

Datum
intset_in(PG_FUNCTION_ARGS)
{
	char *str = PG_GETARG_CSTRING(0);
	Intset *res = parse_set(str);
	if (res == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for int set: \"%s\"",
						str)));
	PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(intset_out);

Datum
intset_out(PG_FUNCTION_ARGS)
{
	Intset *s = (Intset *) PG_GETARG_POINTER(0);
	char	   *result;
	int len = VARSIZE(s);
	int *arr; 
	memcpy(arr,s->data,len - VARHDRSZ)
	
	result = psprintf("(%g,%g)", complex->x, complex->y);
	PG_RETURN_CSTRING(result);
}
