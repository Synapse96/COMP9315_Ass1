#include "postgres.h"
#include "fmgr.h"

PG_MODULE_MAGIC;

typedef struct Intset
{
	char v1_len[4];
	char data[1];
	int cdn; //cardinality?
} Intset;

Intset *parse_set(char *str);
int internal_cmpfunc (const void * a, const void * b);
int internal_rmv_dup(int *arr, int len);

/*****************************************************************************
 * Input/Output functions
 *****************************************************************************/

PG_FUNCTION_INFO_V1(intset_in);

Datum
intset_in(PG_FUNCTION_ARGS)
{
    char *str = PG_GETARG_CSTRING(0);
    Intset *result = parse_set(str);
    if (result == NULL)
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
    Intset *s = (Intset *) PG_GETARG_VARLENA_P(0);
    int len = VARSIZE_ANY_EXHDR(s) / 4;
    if (len == 0) {
        PG_RETURN_CSTRING("{}");
    }
    int arr[len];
    int i;
    memcpy(arr,s->data,VARSIZE_ANY_EXHDR(s));
    char *result =  (char *)palloc(VARSIZE(s));
    if (len == 1) {
        sprintf(result,"{%d}",arr[0]);
        PG_RETURN_CSTRING(result);
    } else {
        sprintf(result,"{%d,",arr[0]);
    }
    char *temp =  (char *)palloc(40);
    for(i = 1; i < len - 1; i++) {
        sprintf(temp,"%d,",arr[i]);
        strcat(result,temp);
    }
    sprintf(temp,"%d}",arr[i]);
    strcat(result,temp);
    PG_RETURN_CSTRING(result);
}


Intset *parse_set(char *str) {
    int arr[strlen(str)];
    Intset *s;
    int j = 0, temp = 0, isNeg = 0;
    int numFound = 0;
    int i;
    for(i = 1; i < strlen(str); i++) {
        if (str[i] == ' ') {
            if (numFound == 1) {
                numFound = 2;
            } 
            continue;
        } else if (str[i] == ',') {
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
        } else if (str[i] == '}' && i == strlen(str) - 1) {
            if (numFound != 0) {
                arr[j] = temp; 
                j++;
            }
        } else {
            return NULL;
        }
    }
    qsort(arr, j, sizeof(int), internal_cmpfunc);
    j = internal_rmv_dup(arr,j);
    
    s = (Intset *)palloc(j*sizeof(Intset) + 4);
    SET_VARSIZE(s,(4*j) + VARHDRSZ);
    memcpy(s->data,arr,4*j);
    return s;
}

/*****************************************************************************
 * Operators
 *****************************************************************************/

int internal_rmv_dup(int *arr, int len) {

  if(len == 0 || len == 1) {
    return len;
  }

  int temp[len];
  int i, j = 0;

  for(i = 0; i < len - 1 ; i++) {
    if(arr[i] != arr[i + 1]) {
      temp[j] = arr[i];
      j++;
    }
  }
  temp[j] = arr[len - 1];
  j++;

  for(i = 0; i < j; i++) {
    arr[i] = temp[i];
  }
  return j;
}

int internal_cmpfunc (const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}

static bool
intset_con_internal(int i, Intset * a)
{
    int len = VARSIZE_ANY_EXHDR(a) / 4;
    int arr[len];
    memcpy(arr, a->data, VARSIZE_ANY_EXHDR(a));
    
    int j;
    for (j = 0; j < len; j++) {
        if (arr[j] == i) {
            return true;
        }
    }
    return false;
}

static bool
intset_sub_internal(Intset * a, Intset * b)
{
    int len = VARSIZE_ANY_EXHDR(a) / 4;
    int arr[len];
    memcpy(arr, a->data, VARSIZE_ANY_EXHDR(a));
    
    int i;
    for (i = 0; i < len; i++) {
        if (!(intset_con_internal(arr[i], b))) {
            return false;
        }
    }
    return true;
}

PG_FUNCTION_INFO_V1(intset_con);

Datum
intset_con(PG_FUNCTION_ARGS)
{
    int i = PG_GETARG_INT32(0);
    Intset *a = (Intset *) PG_GETARG_VARLENA_P(1);
    
    PG_RETURN_BOOL(intset_con_internal(i, a));
}

PG_FUNCTION_INFO_V1(intset_cdn);

Datum
intset_cdn(PG_FUNCTION_ARGS)
{
    Intset *a = (Intset *) PG_GETARG_VARLENA_P(0);
    int len = VARSIZE_ANY_EXHDR(a) / 4;
    
    PG_RETURN_INT32(len);
}

PG_FUNCTION_INFO_V1(intset_sub);

Datum
intset_sub(PG_FUNCTION_ARGS)
{
    Intset *a = (Intset *) PG_GETARG_VARLENA_P(0);
    Intset *b = (Intset *) PG_GETARG_VARLENA_P(1);
    
    PG_RETURN_BOOL(intset_sub_internal(a, b));
}

PG_FUNCTION_INFO_V1(intset_eq);

Datum
intset_eq(PG_FUNCTION_ARGS)
{
    Intset *a = (Intset *) PG_GETARG_VARLENA_P(0);
    Intset *b = (Intset *) PG_GETARG_VARLENA_P(1);
    
    PG_RETURN_BOOL(intset_sub_internal(a, b) && intset_sub_internal(b, a));
}

PG_FUNCTION_INFO_V1(intset_int);

Datum
intset_int(PG_FUNCTION_ARGS)
{
    Intset *a = (Intset *) PG_GETARG_VARLENA_P(0);
    Intset *b = (Intset *) PG_GETARG_VARLENA_P(1);
    Intset *newset;
    
    int a_len = VARSIZE_ANY_EXHDR(a) / 4;
    int b_len = VARSIZE_ANY_EXHDR(b) / 4;
    int arr[a_len];
    memcpy(arr, a->data, VARSIZE_ANY_EXHDR(a));
    
    int max_size = (a_len <= b_len) ? a_len : b_len;
    int temp[max_size];
    int count = 0;
    int i;
    for (i = 0; i < a_len; i++) {
        if (intset_con_internal(arr[i], b)) {
            temp[count] = arr[i];
            count = count + 1;
        }
    }
    
    int newdata[count];
    int j;
    for (j = 0; j < count; j++) {
        newdata[j] = temp[j];
    }
    
    newset = (Intset *) palloc(count * sizeof(Intset) + 4);
    SET_VARSIZE(newset, (4 * count) + VARHDRSZ);
    memcpy(newset->data, newdata, count * 4);
    PG_RETURN_POINTER(newset);
}

PG_FUNCTION_INFO_V1(intset_uni);

Datum
intset_uni(PG_FUNCTION_ARGS)
{
    Intset *a = (Intset *) PG_GETARG_VARLENA_P(0);
    Intset *b = (Intset *) PG_GETARG_VARLENA_P(1);
    Intset *newset;
    
    int a_len = VARSIZE_ANY_EXHDR(a) / 4;
    int b_len = VARSIZE_ANY_EXHDR(b) / 4;
    int a_arr[a_len];
    int b_arr[b_len];
    memcpy(a_arr, a->data, VARSIZE_ANY_EXHDR(a));
    memcpy(b_arr, b->data, VARSIZE_ANY_EXHDR(b));
    
    int max_size = a_len + b_len;
    int temp[max_size];
    int count = 0;
    int i, j;
    for (i = 0; i < a_len; i++) {
        temp[count] = a_arr[i];
        count = count + 1;
    }
    for (j = 0; j < b_len; j++) {
        temp[count] = b_arr[j];
        count = count + 1;
    }
    
    int newdata[count];
    int k;
    for (k = 0; k < count; k++) {
        newdata[k] = temp[k];
    }
    
    newset = (Intset *) palloc(count * sizeof(Intset) + 4);
    SET_VARSIZE(newset, (4 * count) + VARHDRSZ);
    memcpy(newset->data, newdata, count * 4);
    PG_RETURN_POINTER(newset);
}

PG_FUNCTION_INFO_V1(intset_dis);

Datum
intset_dis(PG_FUNCTION_ARGS)
{
    Intset *a = (Intset *) PG_GETARG_VARLENA_P(0);
    Intset *b = (Intset *) PG_GETARG_VARLENA_P(1);
    Intset *newset;
    
    int a_len = VARSIZE_ANY_EXHDR(a) / 4;
    int b_len = VARSIZE_ANY_EXHDR(b) / 4;
    int a_arr[a_len];
    int b_arr[b_len];
    memcpy(a_arr, a->data, VARSIZE_ANY_EXHDR(a));
    memcpy(b_arr, b->data, VARSIZE_ANY_EXHDR(b));
    
    int max_size = a_len + b_len;
    int temp[max_size];
    int count = 0;
    int i, j;
    for (i = 0; i < a_len; i++) {
        if (!(intset_con_internal(a_arr[i], b))) {
            temp[count] = a_arr[i];
            count = count + 1;
        }
    }
    for (j = 0; j < b_len; j++) {
        if (!(intset_con_internal(b_arr[i], a))) {
            temp[count] = b_arr[j];
            count = count + 1;
        }
    }
    
    int newdata[count];
    int k;
    for (k = 0; k < count; k++) {
        newdata[k] = temp[k];
    }
    
    newset = (Intset *) palloc(count * sizeof(Intset) + 4);
    SET_VARSIZE(newset, (4 * count) + VARHDRSZ);
    memcpy(newset->data, newdata, count * 4);
    PG_RETURN_POINTER(newset);
}

PG_FUNCTION_INFO_V1(intset_dif);

Datum
intset_dif(PG_FUNCTION_ARGS)
{
    Intset *a = (Intset *) PG_GETARG_VARLENA_P(0);
    Intset *b = (Intset *) PG_GETARG_VARLENA_P(1);
    Intset *newset;
    
    int len = VARSIZE_ANY_EXHDR(a) / 4;
    int arr[len];
    memcpy(arr, a->data, VARSIZE_ANY_EXHDR(a));
    
    int max_size = len;
    int temp[max_size];
    int count = 0;
    int i;
    for (i = 0; i < len; i++) {
        if (!(intset_con_internal(arr[i], b))) {
            temp[count] = arr[i];
            count = count + 1;
        }
    }
    
    int newdata[count];
    int j;
    for (j = 0; j < count; j++) {
        newdata[j] = temp[j];
    }
    
    newset = (Intset *) palloc(count * sizeof(Intset) + 4);
    SET_VARSIZE(newset, (4 * count) + VARHDRSZ);
    memcpy(newset->data, newdata, count * 4);
    PG_RETURN_POINTER(newset);
}
