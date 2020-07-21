/*	
	File			:	iirHW_AcceleratorDriver.c 
	
	Purpose			:	Implment a wrapper code to drive IIR HW accelerator in a Digital Signal Processor (DSP)  
					
	Specifications	:	Input is Signed Integer
						IIR Accelerator HW Unit in the DSP is available
						No Of Channels = 8
						No Of SOS Stages for each channel is 8.
						Filter coefficients for each of the channel can be programmed independantly
	
	This program is submitted as part of the technical collateral for DSP Engineering position with Cadence Design Systems 
	Ref : Technical Discussion with Mr. Sachin Ghanekar & Shreekanta Datta MN, 20Jul2020.  

*/	


#include <stdio.h>
#include <stdlib.h> 
#include <math.h>
#include <string.h> 


// Definitions 
#define BUFFER_SIZE				2048		// Just to mimic a long sequence of input 
#define FRAME_LENGTH			32			// The input is processed in blocks of 32 samples aka Frame Based Processing 
#define TOTAL_NO_OF_FRAMES		(BUFFER_SIZE / FRAME_LENGTH) 
#define NO_OF_CHANNELS			8			//	
#define NO_OF_SOS_STAGES		8			// Number of cascaded SOS Stages for each channel   
#define SOS_HIST_SIZE			2			// Number of history points required for each SOS stage  (s1 & s2) 
#define NO_OF_COEFFS_PER_SOS	5			// In order : b0,b1,b2,a1,a2 	


static int audioInput[NO_OF_CHANNELS][BUFFER_SIZE];		// Emulates the data from a TDM input stream into the DSP 
static int audioOutput[NO_OF_CHANNELS][BUFFER_SIZE];	// Emulates the data to a TDM output stream out of the DSP 

static int inputBuf[NO_OF_CHANNELS][FRAME_LENGTH];		// A block of audio of size FRAME_LENGTH is pulled from the audioInput and copied into inputBuf 
static int outputBuf[NO_OF_CHANNELS][FRAME_LENGTH];		// A block of audio of size FRAME_LENGTH is copied from from the outputBuf to  audioOutput

static int sosCoeffBuff[NO_OF_CHANNELS][NO_OF_SOS_STAGES][NO_OF_COEFFS_PER_SOS];
static int sosHistBuf[NO_OF_CHANNELS][NO_OF_SOS_STAGES][SOS_HIST_SIZE];


// Function Declarations
void iirHWAcclDriver(void); 
// void iirHWAcclDriver(int ch);								// If called channel-wise 	
int iirSoS(int inpSample, int* sosCoeffPtr, int* histPtr);		// core IIR SOS Routine 

int main()
{		
	int i,frame,ch,smpl,sos;
	
	// Populate or Generate dummy input
	srand(1); 
	for (ch = 0; ch < NO_OF_CHANNELS; ch++)
	{		
		for (smpl = 0; smpl < BUFFER_SIZE; smpl++)
		{	
			audioInput[ch][smpl] = (int) (rand() % 100); 
		}	
	}

	// Initialize Coefficient Buffers with some random values 
	for (ch = 0; ch < NO_OF_CHANNELS; ch++)
	{		
		for (sos = 0; sos < NO_OF_SOS_STAGES; sos++)
		{	
			for (i = 0; i < NO_OF_COEFFS_PER_SOS; i++)
			{
				sosCoeffBuff[ch][sos][i] = (int)(-rand() % 50);
			}
		}	
	}		


	// Copy data from Audio Buffer to InputBuffer and call the IIR Driver 		
	for (frame = 0; frame < TOTAL_NO_OF_FRAMES; frame+= FRAME_LENGTH)
	{		
		for (ch = 0; ch < NO_OF_CHANNELS; ch++)
		{	
			memcpy(&inputBuf[ch][0], &audioInput[ch][frame], FRAME_LENGTH * sizeof(int)); 
			// iirHWAcclDriver(ch);		
		}
		iirHWAcclDriver();		// Or call the driver in the inner loop above for that particular channel only  
	}		

	printf(" Thanks for trying this usage of IIR HW Acclerator Demo program and hope this was useful !! \n"); 
}



/*	
	-------------------------------------------------------------------------------------------------------------------------
	Function	:	iirHWAcclDriver

	Brief		:	This functions assumes that all the input buffers are filled and valid for all channels before the call.  
					The coefficient buffer for each of the SOS stages are pre loaded as per the requirements.   
					The SOS history buffers are reserved for the exclusive use of iirSOS() biquads. 

	Arguments	:	None 
	
	Returns		:	None 
					All the input arguments and the outputs are declared globals for demo purpose  
	-------------------------------------------------------------------------------------------------------------------------

*/		
void iirHWAcclDriver(void) 
{	
	int chIndex, smplIndex, sosIndex, inputSample, tmpInOut;
	
	for (chIndex = 0; chIndex < NO_OF_CHANNELS; chIndex++)
	{
		for (smplIndex = 0; smplIndex < FRAME_LENGTH; smplIndex++)
		{
			tmpInOut = inputBuf[chIndex][smplIndex];	// For the very first SOS stage input = x[0] 
			for (sosIndex = 0; sosIndex < NO_OF_SOS_STAGES; sosIndex++)
			{										
				tmpInOut = iirSoS(tmpInOut, &sosCoeffBuff[chIndex][sosIndex][0], &sosHistBuf[chIndex][sosIndex][0]);
			}										
			outputBuf[chIndex][smplIndex] = tmpInOut;	// The filtered output of the last SOS stage is the final y[n] for x[n]   
		}
	}
	
	return;
}	
	
/*	
	----------------------------------------------------------------------------------------------------
	SOS Equations in Transposed DF-2  
	y[n]	=  	b0*x[n] + s1				
	s1		= 	b1*x[n] - a1*y[n] + s2;		
	s2		= 	b2*x[n] - a2*y[n]			

	If 'a1' and 'a2' are already negated and stored, then s1 and s2 equaation become all additions  
	For the Cascaded SOS output tmpSampleOut of one SOS is the input x[n] to the next SOS. 
	The tmpSampleOut output of the last SOS stage is the final output y[n] for the input sample x[n]
	---------------------------------------------------------------------------------------------------
*/
int iirSoS(int inpSample, int *sosCoeffPtr, int *histPtr)
{
	int tmpSampleOut=0; 

	tmpSampleOut	= (sosCoeffPtr[0] * inpSample) + histPtr[0]; 
	histPtr[0]		= (sosCoeffPtr[1] * inpSample) - (sosCoeffPtr[3] * tmpSampleOut) + histPtr[1];
	histPtr[1]		= (sosCoeffPtr[2] * inpSample) - (sosCoeffPtr[4] * tmpSampleOut);

	return tmpSampleOut; 
}


/* 
	---------------------------------------------------------------------------------------------------------------------------
	Notes:	
		1.	There are intermediate values in iirSoS where there are overflow and underflow  	
		2.	No optimization is attempted and the driver code populates random values for input and filter co-efficients 
		3.  This was tested with Microsoft Visual C++ 2019 

	Things to Be Done : 
		1.	Implement a Matlab Code and create a 8 SOS filter structure per channel and each channels output is a band pass filter  
		2.	Input a frequency sweep to each of channel and verify that the combination of 8 channels acts as a filter bank.   
		3.	Write a Floating point version of this C Code and ensure that the test results of (2) match the Matlab implementation
		4.	Convert the floating point C code to 16/32 bit fixed point implementation and match the results  
		5.	Instead of IIR HW Accelerator optimize the implementation assuuming SIMD engine and verify the results 
		7.	For a two channel implementation, record and compare the MCPS of both SIMD Vs IIR HW Accelerator   
		8.	Upload the code, test results to Github  https://github.com/dattamns/digitalSignalProcessing
	----------------------------------------------------------------------------------------------------------------------------
*/	