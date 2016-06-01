
/* Author: Nicholas DeCicco <decicco@ucar.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <netcdf.h>

#include "gbytes.cpp"

// Rounds up division.
#define DIV_CEIL(n,d) (((n)-1)/(d)+1)

#define LINE_LENGTH 100
#define HEADER_LINES 11

#define GET_COMP_HEADER_SIZE(l) (DIV_CEIL((l)*LINE_LENGTH*6,8))
#define GET_DECOMP_HEADER_SIZE(l) ((l)*LINE_LENGTH)

// The size of the header, in bytes, when compressed (i.e., when encoded).
#define COMP_HEADER_SIZE GET_COMP_HEADER_SIZE(HEADER_LINES)
#define DECOMP_HEADER_SIZE GET_DECOMP_HEADER_SIZE(HEADER_LINES)
#define PARAMETER_HEADER_START COMP_HEADER_SIZE

enum {
	kFalse = 0,
	kTrue = 1
};

typedef struct {
	int rate;         /** Sample rate in samples per "program cycle" (which can
	                      vary). */
	float scale;      /** Scale to be applied to reconstruct original parameter
	                      values. */
	float bias;       /** Offset to be applied to reconstruct original
	                      parameter values. */
	char *label;      /** The label associated with this parameter. */
	char *desc;       /** Description text associated with this parameter. */
	size_t descLen;   /** Length of description text, not including the null
	                      terminator. */
	char *units;      /** Units text associated with this parameter. */
	size_t unitsLen;  /** Length of units text, not including the null
	                      terminator. */
	int ncVar;        /** The NetCDF variable ID corresponding to this
	                      parameter. */
	float *values;    /** A pointer to an array of values for this
	                      parameter. */
	size_t numValues; /** Total number of samples recorded. This is the length
	                      of the `values' array. */
	char isUnused;    /** Indicates if the variable is unused. */
	int ncDimId;      /** Handle to the NetCDF dimension for this array. */
} Parameter;

int read_header_chunk(FILE *fp, uint8_t **in_buffer, char **header_decomp, int numLines);
int get_text(char *const in_buf,
             const int offset,
             const int maxLength,
             char **out_buf,
             size_t *const out_length);

int main(int argc, char **argv)
{
	int status;
	int ncid; /* Handle on the NetCDF file */
	size_t blockLength, dataStart, numRecords, amtRead;
	char name[100];
	int cmode = NC_NOCLOBBER;
	FILE *fp;
	char *inFileName, *outFileName;
	uint8_t *in_buffer = NULL;
	int *data_decomp = NULL;
	char *header_decomp = NULL;
	int i, j, k, m;
	int start, curCycle;
	Parameter *params = NULL;
	int numParameters, numPerCycle, cyclesPerBlock;
	float cycleTime;
	char *fileDesc;     /* File description text. */
	size_t fileDescLen; /* Length of the file description text. */

	if (argc != 3) {
		fprintf(stderr, "Error: Require exactly two arguments.\n");
		printf("Usage:\n"
		       "\n"
		       "    convert INFILE OUTFILE\n");
		exit(1);
	}

	inFileName = argv[1];
	outFileName = argv[2];

	if (!(fp = fopen(inFileName, "r"))) {
		fprintf(stderr, "Error: Failed to open \"%s\" for reading.\n", inFileName);
		exit(1);
	}

	if (!read_header_chunk(fp, &in_buffer, &header_decomp, HEADER_LINES)) {
		fclose(fp);
		exit(1);
	}

	// Get the file description.
	if (!get_text(header_decomp, 0, 23, &fileDesc, &fileDescLen)) {
		// TODO: cleanup
		return 1;
	}

	sscanf(header_decomp+175, "%3d", &numParameters);
	sscanf(header_decomp+246, "%4d", &numPerCycle);
	sscanf(header_decomp+290, "%f", &cycleTime);
	sscanf(header_decomp+304, "%d", &cyclesPerBlock);

	if (!(params = (Parameter*) malloc(sizeof(Parameter)*numParameters))) {
		free(in_buffer);
		free(header_decomp);
		fclose(fp);
		goto mallocfail;
	}
	memset(params, 0, sizeof(Parameter)*numParameters);

	/* Now that we know precisely how many parameters there are, we can
	 * decompress the parameter description text.
	 */
	fseek(fp, COMP_HEADER_SIZE, SEEK_SET);
	if (!read_header_chunk(fp, &in_buffer, &header_decomp, numParameters)) {
		fclose(fp);
		exit(1);
	}

	/*
	 * Parse parameters' parameters.
	 */

#define PARAMETER(i) (header_decomp+LINE_LENGTH*(i))

	for (i = 0; i < numParameters; i++) {
		sscanf(PARAMETER(i)+4, "%d", &(params[i].rate));
		sscanf(PARAMETER(i)+80, "%f", &(params[i].scale));
		sscanf(PARAMETER(i)+90, "%f", &(params[i].bias));

		// Get the parameter (variable) name.
		if (!get_text(header_decomp, LINE_LENGTH*i+56, 9,
		              &(params[i].label), NULL))
		{
			goto param_malloc_fail;
		}

		// Skip unused parameters
		if (strcmp(params[i].label, "UNUSED") == 0) {
			params[i].isUnused = kTrue;
		}

		// Get the description text.
		if (!get_text(header_decomp, LINE_LENGTH*i+13, 42,
		              &(params[i].desc), &(params[i].descLen)))
		{
			goto param_malloc_fail;
		}

		// Get the units text.
		if (!get_text(header_decomp, LINE_LENGTH*i+66, 7,
		              &(params[i].units), &(params[i].unitsLen)))
		{
			goto param_malloc_fail;
		}
	}

	free(header_decomp);
	header_decomp = NULL;

	/*
	 * Determine how many records there are by doing some math.
	 */
	fseek(fp, 0L, SEEK_END);

	// Offset to the start of data.
	dataStart = DIV_CEIL((HEADER_LINES+numParameters)*LINE_LENGTH*6,64)*8;

	// Amount of data to read in with each fread(). (I.e., the amount of data
	// in one cycle's worth of samples.)
	blockLength = DIV_CEIL(cyclesPerBlock*numPerCycle*20/*bits per value*/,64) *
	             8 /*bytes per 64-bit word*/;

	// It would seem that if cyclesPerBlock*numPerCycle*20%64 == 0 (i.e., the
	// amount of space needed for a block is exactly divisible by 64 such that
	// the data would run right up to the edge with no zero padding), a word
	// of zero padding is added. (So every block is separated by 8 zero bytes.)
	if (cyclesPerBlock*numPerCycle*20/*bits per value*/ % 64 == 0) {
		blockLength += 8;
	}

	// Number of records in the file.
	numRecords = DIV_CEIL(ftell(fp) - dataStart, blockLength);

	assert((size_t) ftell(fp) == numRecords*blockLength+dataStart);

	/*
	 * Get data from the file.
	 */

	if (!(in_buffer = (uint8_t*) realloc(in_buffer, sizeof(uint8_t)*blockLength))) {
		goto mallocfail;
	}
	if (!(data_decomp = (int*) realloc(data_decomp, sizeof(int)*cyclesPerBlock*numPerCycle))) {
		goto mallocfail;
	}

	// Allocate arrays
	for (i = 0; i < numParameters; i++) {
		if (params[i].isUnused) continue;
		params[i].numValues = params[i].rate * numRecords * cyclesPerBlock;
		if (!(params[i].values =
		      (float*) malloc(sizeof(float)*params[i].numValues)))
		{
			goto mallocfail;
		}
	}

	// Next, read out the records
	fseek(fp, dataStart, SEEK_SET);
	curCycle = 0; // Which cycle are we reading?
	do {
		amtRead = fread(in_buffer, sizeof(uint8_t), blockLength, fp);
		if (amtRead < blockLength) {
			if (amtRead > 0) {
				fprintf(stderr, "Warning: premature EOF encountered while "
				        "reading data\n");
			}
			break;
		}
		gbytes<uint8_t,int>(in_buffer, data_decomp, 0, 20, 0, cyclesPerBlock*numPerCycle);
		k = 0; // Index into data_decomp
		for (m = 0; m < cyclesPerBlock; m++) {
			for (i = 0; i < numParameters; i++) {
				if (params[i].isUnused) {
					k += params[i].rate;
					continue;
				}
				start = params[i].rate*curCycle;
				for (j = 0; j < params[i].rate; j++) {
					params[i].values[start+j] = data_decomp[k++];
				}
			}
			curCycle++;
		}
	} while (!feof(fp));

	free(in_buffer); in_buffer = NULL;
	free(data_decomp); data_decomp = NULL;
	fclose(fp);

	// Restore original values by scaling/biasing.
	for (i = 0; i < numParameters; i++) {
		if (params[i].isUnused) continue;
		for (j = 0; j < (int) params[i].numValues; j++) {
			params[i].values[j] = ((float) params[i].values[j])/params[i].scale -
			                      params[i].bias;
		}
	}

	/*
	 * Save the data to NetCDF.
	 */

	if ((status = nc_create(outFileName, cmode, &ncid)) != NC_NOERR) {
		fprintf(stderr, "Fatal: failed to create NetCDF file.\n");
		goto ncerr;
	}

	// Define dimensions and variables.
	for (i = 0; i < numParameters; i++) {
		if (params[i].isUnused) continue;
		sprintf(name, "%s_LENGTH", params[i].label);
		if ((status = nc_def_dim(ncid, name, params[i].numValues,
		                         &(params[i].ncDimId))) != NC_NOERR)
		{
			fprintf(stderr, "Fatal: failed to define dimension for %s\n",
			        params[i].label);
			goto ncerr;
		}
		if ((status = nc_def_var(ncid, params[i].label, NC_FLOAT, 1L,
		                         &(params[i].ncDimId),
		                         &(params[i].ncVar))) != NC_NOERR)
		{
			fprintf(stderr, "NetCDF variable definition failed, aborting.\n");
			goto ncerr;
		}

		if ((status = nc_put_att_text(ncid, params[i].ncVar, "DESCRIPTION",
		                              params[i].descLen,
		                              params[i].desc)) != NC_NOERR)
		{
			goto ncerr;
		}

		if ((status = nc_put_att_text(ncid, params[i].ncVar, "UNITS",
		                              params[i].unitsLen,
		                              params[i].units)) != NC_NOERR)
		{
			goto ncerr;
		}

		if ((status = nc_put_att_int(ncid, params[i].ncVar, "SAMPLE_RATE",
		                             NC_INT, 1,
		                             &(params[i].rate))) != NC_NOERR)
		{
			goto ncerr;
		}

	}

	if ((status = nc_put_att_text(ncid, NC_GLOBAL, "DESCRIPTION",
	                              fileDescLen, fileDesc)) != NC_NOERR)
	{
		goto ncerr;
	}

	if ((status = nc_put_att_float(ncid, NC_GLOBAL, "CYCLE_TIME", NC_FLOAT, 1,
	                               &cycleTime)) != NC_NOERR)
	{
		goto ncerr;
	}

	nc_enddef(ncid);

	for (i = 0; i < numParameters; i++) {
		if (params[i].isUnused) continue;
		if (nc_put_var_float(ncid, params[i].ncVar,
		                     params[i].values) != NC_NOERR)
		{
			exit(1);
		}
	}

	nc_close(ncid);

	if (fileDesc) { free(fileDesc); fileDesc = NULL; }

	// Clean up.
	for (i = 0; i < numParameters; i++) {
		if (params[i].values) { free(params[i].values); params[i].values = NULL; }
		if (params[i].label)  { free(params[i].label);  params[i].label  = NULL; }
		if (params[i].desc)   { free(params[i].desc);   params[i].desc   = NULL; }
		if (params[i].units)  { free(params[i].units);  params[i].units  = NULL; }
	}
	free(params);
	params = NULL;

	return 0;

param_malloc_fail: // Memory allocation failed while reading parameter info
	for (j = 0; j < i; j++) {
		if (params[i].label) free(params[i].label);
				if (params[i].desc) free(params[i].desc);
		if (params[i].units) free(params[i].units);
	}
	fclose(fp);
	if (in_buffer) free(in_buffer);
	if (header_decomp) free(header_decomp);
	free(params);
	// fall through

mallocfail:
	fprintf(stderr, "Memory allocation failed, aborting.\n");
	return 1;

ncerr:
	fprintf(stderr, "Error string was: %s\n", nc_strerror(status));
	return 1;
}

int read_header_chunk(FILE *fp, uint8_t **in_buffer, char **header_decomp, int numLines)
{
	size_t i;
	size_t compSize = GET_COMP_HEADER_SIZE(numLines);
	size_t decompSize = GET_DECOMP_HEADER_SIZE(numLines);
	char ascii[65 /* add one for the null terminator */] =
		":"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"0123456789"
		"+-*/()$= ,.#[]%\"_!&'?<>@\\^;";

	if (!(*in_buffer = (uint8_t*) realloc(*in_buffer,
	                                      sizeof(uint8_t)*compSize)))
	{
		return 0;
	}

	if (fread(*in_buffer, sizeof(uint8_t), compSize, fp) != compSize) {
		fprintf(stderr, "Error: failed to read header.\n");
		free(in_buffer);
		return 0;
	}

	if (!(*header_decomp = (char*) realloc(*header_decomp,
	                                       sizeof(char)*(decompSize+1))))
	{
		free(in_buffer);
		return 0;
	}

	// Decompress the header.
	gbytes<uint8_t,char>(*in_buffer, *header_decomp, 0, 6, 0, decompSize);

	for (i = 0; i < decompSize; i++) {
		(*header_decomp)[i] = ascii[(int) (*header_decomp)[i]];
	}
	(*header_decomp)[decompSize] = '\0';

	for (i = 0; i < decompSize; i += LINE_LENGTH) {
		fwrite((*header_decomp)+i, sizeof(char), LINE_LENGTH, stdout);
		fputc('\n', stdout);
	}

	return 1;
}

int get_text(char *const in_buf,
             const int offset,
             const int maxLength,
             char **out_buf,
             size_t *const out_length)
{
	int i;

	for (i = maxLength; in_buf[offset+i] == ' ' && i > -1; i--) {}
	i++;
	if (!(*out_buf = (char*) malloc(sizeof(char)*(i+1)))) {
		return 0;
	}
	strncpy(*out_buf, in_buf+offset, i);
	(*out_buf)[i] = '\0';
	if (out_length) *out_length = i;
	return 1;
}
