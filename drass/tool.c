#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "file_dir.h"
#include "bloom.h"
#include "tool.h"

int
fastq_read_check (char *begin, int length, char *model, bloom * bl, float tole_rate)
{ 
  char *p = begin;
  
  int distance = length;
  int signal = 0, result = 0;
  char *previous, *key = (char *) malloc (bl->k_mer * sizeof (char) + 1);

  while (distance > 0)
    {
      if (signal == 1)
	       break;

      if (distance >= bl->k_mer)
	{
	  memcpy (key, p, sizeof (char) * bl->k_mer);	//need to be tested
	  key[bl->k_mer] = '\0';
	  previous = p;
	  p += bl->k_mer;
	  distance -= bl->k_mer;
	}

      else
	{
	  memcpy (key, previous + distance, sizeof (char) * bl->k_mer);
	  p += (bl->k_mer - distance);
	  signal = 1;
	}

      if (model == "reverse")
	        rev_trans (key);      

	  if (bloom_check (bl, key))
	    { 
	      result =  fastq_full_check (bl, begin, length, model, tole_rate);
              if (result > 0)
                  return result;
              else if (model == "normal")
                  break;
	    }

    }				//outside while
    if (model == "reverse")
        return 0;
    else
        return fastq_read_check (begin, length, "reverse", bl,  tole_rate);
}

/*-------------------------------------*/
int
fastq_full_check (bloom * bl, char *p, int distance, char *model, float tole_rate)
{

  printf("fastq full check...\n");

  int length = distance;

  int count = 0, match_s = 0, mark = 1;

  float result;

  char *previous, *key = (char *) malloc (bl->k_mer * sizeof (char) + 1);
  
  while (distance >= bl->k_mer)
    {
      memcpy (key, p, sizeof (char) * bl->k_mer);
      key[bl->k_mer] = '\0';
      previous = p;
      p += 1;
      
      if (model == "reverse")
	        rev_trans (key);
     
         if (count >= bl->k_mer)
         {
           mark = 1;
           count = 0;
         }
	if (bloom_check (bl, key))
	  { 
            //printf("hit--->\n");
            if (mark == 1)
                {
                match_s+=(bl->k_mer-1);
                mark = 0;
                }
            else
                match_s++;
	  }
      else
         //printf("unhit-->\n");
      distance--;
    }				// end while
  free (key);
  result = (float) match_s / (float) length; 
  if (result >= tole_rate)
    return match_s;
  else
    return 0;
}

int
fasta_read_check (char *begin, char *next, char *model, bloom * bl, float tole_rate)
{

  char *p = strchr (begin + 1, '\n') + 1;

  if (!p || *p == '>')
    return 1;

  char *start = p;

  int n, m, result, count_enter;

  char *key = (char *) malloc ((bl->k_mer + 1) * sizeof (char));

  char *pre_key = (char *) malloc ((bl->k_mer + 1) * sizeof (char));

  key[bl->k_mer] = '\0';

  while (p != next)
    {
      while (n < bl->k_mer)
	{
	  if (p[m] == '>' || p[m] == '\0')
	    {
	      m--;
	      break;
	    }

	  if (p[m] != '\r' && p[m] != '\n')
	    key[n++] = p[m];
	  else
	    count_enter++;
	  m++;
	}			//inner while

      if (m == 0)
	break;

      if (strlen (key) == bl->k_mer)
	memcpy (pre_key, key, sizeof (char) * (bl->k_mer + 1));

      else
	{
	  char *temp_key = (char *) malloc (bl->k_mer * sizeof (char));

	  memcpy (temp_key, pre_key + strlen (key), bl->k_mer - strlen (key));

	  memcpy (temp_key + bl->k_mer - strlen (key), key,
		  sizeof (char) * (strlen (key) + 1));

	  free (key);

	  key = temp_key;

	}
      p += m;

      n = 0;

      m = 0;

      if (model == "reverse")
	rev_trans (key);

	  if (bloom_check (bl, key))
	    {
	      result =  fasta_full_check (bl, begin, next, model, tole_rate);
              if (result > 0)
                  return result;
              //else if (model == "normal")	//use recursion to check the sequence forward and backward
              //    return fasta_read_check (begin, next, "reverse", bl);
              else if (model == "normal")
                  break;
	    }

      //memset (key, 0, bl->k_mer);
    }				//outside while
    if (model == "reverse")
        return 0;
    else
        return fasta_read_check (begin, next, "reverse", bl, tole_rate);
}  

/*-------------------------------------*/
int
fasta_full_check (bloom * bl, char *begin, char *next, char *model, float tole_rate)
{
  int match_s = 0, count = 0, mark = 1;

  int n = 0, m = 0, count_enter = 0;

  float result = 0;

  char *key = (char *) malloc ((bl->k_mer + 1) * sizeof (char));

  begin = strchr (begin + 1, '\n') + 1;

  char *p = begin;

  while (p != next)
    {
      if (*p == '\n')
	count_enter++;
      p++;
    }

  p = begin;

  while (*p != '>' && *p != '\0')
    {
      while (n < bl->k_mer)
	{
	  if (p[m] == '>' || p[m] == '\0')
	    {
	      m--;
	      break;
	    }

	  if (p[m] != '\r' && p[m] != '\n')
	    key[n++] = p[m];

	  m++;
	}
      key[n] = '\0';

      if (model == "reverse")
	rev_trans (key);
      //printf("key->%s\n",key);
      if (count >= bl->k_mer)
         {
           mark = 1;
           count = 0;
         }
      if (strlen (key) == bl->k_mer)
       {
	if (bloom_check (bl, key))
	  { 
            //printf("hit--->\n");
            if (mark == 1)
                {
                match_s+=(bl->k_mer-1);
                mark = 0;
                }
            else
                match_s++;
	  }
        
	else
	  {
            //printf("unhit--->\n");
	  }
        
    count++;
        }   //outside if
      //printf("score->%d\n",match_s);
      p++;
      if (p[0] == '\n')
	p++;
      n = 0;
      m = 0;
    }				// end of while
  result = (float)match_s / (float) (next - begin - count_enter);
  if (result >= tole_rate)	//match >tole_rate considered as contaminated
    return match_s;
  else
    return 0;
}
