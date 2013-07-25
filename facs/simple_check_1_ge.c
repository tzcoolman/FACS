#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libgen.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "tool.h"
#include "check.h"
#include "bloom.h"
#include "file_dir.h"

#ifndef __clang__ 
#include <omp.h>
#endif
char *clean, *contam, *clean2, *contam2;

/*save it for the possible advanced version*/
void init_string(int chunk)
{
	clean = (char *) calloc (chunk, sizeof (char));
	contam = (char *) calloc (chunk, sizeof (char));
}

void
fastq_process (bloom * bl, Queue * info, Queue * tail, F_set * File_head, float sampling_rate, float tole_rate, char mode, int print_flag)
{
	char *start_point = info->location;
	char *next_job = NULL, *temp = NULL, *temp_piece = NULL, *previous_point = NULL;
	int result = 0;
	// initialize pointers
	if(info->location[0] != '@')
	{
		return;
	// check if job is empty
	}
	else if(info->next != tail && info->next->location != NULL)
	{
		next_job = info->next->location;
	}
	else
	{
		next_job = strchr (start_point, '\0');
		if (next_job[-1] == '\n' && next_job[-2] == '\n')
			next_job -= 1;
		else if (next_job[-4] == '\r' && next_job[-3] == '\n')
			next_job -= 2;
  	}
	// make sure it can handle DOS and Unix format ('\r\n' and '\n')
	while (start_point != next_job) {
		if (mode == 'c')
		{
			temp = jump (start_point, 2, sampling_rate);	
		// function for fast/proportional scan
			if (start_point != temp)
			{
				start_point = temp;
				continue;
			}
		}
		// skip to the next read if needed
		#pragma omp atomic
		File_head->reads_num++;
		// atomic process for summing reads number
		previous_point = start_point;
		start_point = strchr (start_point, '\n') + 1;
		// skip the ID line of fastq reads
		result = fastq_read_check (start_point, strchr (start_point, '\n') - start_point, 'n', bl, tole_rate, File_head);
		// check the real read line
		start_point = strchr (start_point, '\n') + 1;
		start_point = strchr (start_point, '\n') + 1;
		start_point = strchr (start_point, '\n') + 1;
		// finish scanning this read, skip to the next
		if (mode == 'r')
			if (result>0 && print_flag==0)
			{
				#pragma omp atomic
                        	File_head->reads_contam++;
			//printf("%.*s\n",start_point-previous_point,start_point);
				fprintf(stdout,"%.*s\n",start_point-previous_point,start_point);
			// atomic process for summing captured reads number
			//contam+=remove_data(contam, previous_point, start_point);
			}
			else if (result==0 && print_flag==1)
			{
			//clean+=remove_data(clean, previous_point, start_point);
			//printf("%.*s\n",start_point-previous_point,start_point);
				fprintf(stdout,"%.*s\n",start_point-previous_point,start_point);
			}
	}	// outside while
	if (temp_piece)
		free(temp_piece);
}
/*-------------------------------------*/
void
fasta_process (bloom * bl, Queue * info, Queue * tail, F_set * File_head,
	       float sampling_rate, float tole_rate)
{
#ifdef DEBUG
  printf ("fasta processing...\n");
#endif
  char *temp_next, *next, *temp;

  if (info->location == NULL)
    return;
  else if (info->next != tail)
    next = info->next->location;
  else{
      next = strchr (info->location, '\0');
      //if ((next-1)=='\n')
      //next -= 1;
      if ((next[-1]) == '\n' && next[-2] == '\n')
	next -= 1;
      else if (next[-4] == '\r' && next[-3] == '\n')
	next -= 2;
  }

  char *p = info->location;

  while (p != next) {
      temp = jump (p, 1, sampling_rate);	// generate random number and judge
                                            // if need to scan this read
      if (p != temp)
	{
	  p = temp;
	  continue;
#pragma omp atomic
      File_head->reads_num++;
      temp_next = strchr (p + 1, '>');

      if (!temp_next)
        temp_next = next;

      if (fasta_read_check (p, temp_next, 'n', bl, tole_rate, File_head) > 0) {
#pragma omp atomic
	    File_head->reads_contam++;
	  }

      p = temp_next;
    }
}
}

void
report(char *detail, char *filename, F_set * File_head, char* query, char* fmt, char* prefix)
{
  char buffer[200] = { 0 };
  float contamination_rate = (float) (File_head->reads_contam)/(float) (File_head->reads_num);
  char *save_file = NULL;
  save_file = statistic_save (query, prefix);
  if(!fmt){
      return;
  // JSON output format (via stdout)
  } else if(!strcmp(fmt, "json")) {
      isodate(buffer);

      printf("{\n");
      printf("\t\"timestamp\": \"%s\"\n", buffer);
      printf("\t\"sample\": \"%s\"\n", basename(query)); //sample (query)
      printf("\t\"bloom_filter\": \"%s\"\n", basename(filename)); //reference
      printf("\t\"total_read_count\": %lld,\n", File_head->reads_num);
      printf("\t\"contaminated_reads\": %lld,\n", File_head->reads_contam);
      printf("\t\"total_hits\": %lld,\n", File_head->hits);
      printf("\t\"contamination_rate\": %f,\n", contamination_rate);
      printf("}\n");

  // TSV output format (via file in CWD)
  } else if (!strcmp(fmt, "tsv")) {
      strcat (detail, "sample\tbloom_filter\ttotal_read_count\t\
contaminated_reads\tcontamination_rate\n");

      sprintf(buffer, "%s\t%s\t%lld\t%lld\t%f\n", basename(query),
              basename(filename), File_head->reads_num,
              File_head->reads_contam, contamination_rate);
      strcat(detail, buffer);
      //printf ("name->%s\n",basename(query));
      //write_result(strcat(basename(query), ".tsv"), detail);
      write_result(strcat(save_file, ".tsv"), detail);
  }
}

char *statistic_save (char *filename, char *prefix)
{
  char *save_file = NULL;
  int length = 0;
  //printf ("prefix->%s\n",prefix);
  if (prefix!=NULL && prefix[0]=='.'){
      prefix+=2;
      length = strrchr(prefix,'/')-prefix+1;
      if(length != 0 && strrchr(prefix,'/')!=NULL){
         save_file =(char *) calloc (length, sizeof (char));
         memcpy(save_file,prefix,length);
         prefix = save_file;
         save_file = NULL;
      }
      else{              
         prefix = NULL;
      } 
  }
  if (prefix!=NULL)
      if (prefix[strlen(prefix)-1]=='/')
          prefix[strlen(prefix)-1]='\0'; 
  save_file = prefix_make (filename, NULL, prefix);
  if (is_dir(prefix) || prefix==NULL)
      strcat (save_file, ".info");
  if (strrchr(save_file,'/')==save_file)
      save_file++;
#ifdef DEBUG
  printf ("Basename->%s\n", filename);
  printf ("Info name->%s\n", save_file);
#endif
  return save_file;
}
/*-------------------------------------*/
