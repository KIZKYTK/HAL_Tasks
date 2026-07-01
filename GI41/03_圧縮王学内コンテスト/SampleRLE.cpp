
#include <stdio.h>
#include <string.h>


void Compress(const char* FileName, FILE* CompressFile);
void Decompress(FILE* CompressFile);

unsigned int Encode(unsigned char* OriginalData, unsigned int OriginalSize, unsigned char* CompressData);
unsigned int Decode(unsigned char* CompressData, unsigned int CompressSize, unsigned char* DecompressData);




int main()
{
	printf("1:à≥èkÅ@2:ìWäJ\n");

	int mode;
	scanf("%d", &mode);

	if (mode == 1)
	{
		//à≥èk

		FILE* compressfile;
		compressfile = fopen("compress.dat", "wb");

		Compress("explosion.wav", compressfile);
		Compress("hal.bmp", compressfile);
		Compress("TeraPad.exe", compressfile);
		Compress("wagahaiwa_nekodearu.txt", compressfile);
		Compress("yuuki_256.bmp", compressfile);

		fclose(compressfile);
	}
	else if(mode == 2)
	{
		//ïúçÜ
		
		FILE* compressfile;
		compressfile = fopen("compress.dat", "rb");

		Decompress(compressfile);

		fclose(compressfile);
	}


	return 0;
}





void Compress(const char* FileName, FILE* CompressFile)
{
	FILE* file = fopen(FileName, "rb");
	unsigned int originalSize;
	fseek(file, 0, SEEK_END);
	originalSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	unsigned char* originalData;
	originalData = new unsigned char[originalSize];

	fread(originalData, originalSize, 1, file);
	fclose(file);


	unsigned int compressSize;
	unsigned char* compressData;
	compressData = new unsigned char[originalSize * 2];

	compressSize = Encode(originalData, originalSize, compressData);



	fwrite(FileName, 256, 1, CompressFile);
	fwrite(&originalSize, sizeof(originalSize), 1, CompressFile);
	fwrite(&compressSize, sizeof(compressSize), 1, CompressFile);
	fwrite(compressData, compressSize, 1, CompressFile);

	delete[] originalData;
	delete[] compressData;
}




void Decompress(FILE* CompressFile)
{

	while (true)
	{
		char fileName[256];
		unsigned int originalSize;
		unsigned int compressSize;

		fread(fileName, 256, 1, CompressFile);

		if (feof(CompressFile))
			break;

		fread(&originalSize, sizeof(originalSize), 1, CompressFile);
		fread(&compressSize, sizeof(compressSize), 1, CompressFile);

		unsigned char* compressData;
		compressData = new unsigned char[compressSize];
		fread(compressData, compressSize, 1, CompressFile);



		unsigned int decompressSize;
		unsigned char* decompressData;
		decompressData = new unsigned char[originalSize];

		decompressSize = Decode(compressData, compressSize, decompressData);



		FILE* file = fopen(fileName, "wb");
		fwrite(decompressData, decompressSize, 1, file);
		fclose(file);

		delete[] compressData;
		delete[] decompressData;
	}
}




unsigned int Encode(unsigned char* OriginalData, unsigned int OriginalSize, unsigned char* CompressData)
{
	//ÉâÉìÉåÉìÉOÉXÇ≈à≥èk
	unsigned char byte, nextByte;
	unsigned char count;
	unsigned int compressSize;

	compressSize = 0;

	byte = OriginalData[0];
	count = 0;

	for (unsigned int i = 1; i < OriginalSize; i++)
	{
		nextByte = OriginalData[i];

		if (byte == nextByte && count < 255)
		{
			count++;
		}
		else
		{
			CompressData[compressSize] = byte;
			compressSize++;
			CompressData[compressSize] = count;
			compressSize++;

			byte = nextByte;
			count = 0;
		}
	}

	CompressData[compressSize] = byte;
	compressSize++;
	CompressData[compressSize] = count;
	compressSize++;

	return compressSize;
}





unsigned int Decode(unsigned char* CompressData, unsigned int CompressSize, unsigned char* DecompressData)
{
	//ÉâÉìÉåÉìÉOÉXÇ≈ìWäJ
	unsigned char byte;
	unsigned char count;
	unsigned int current, decompressSize;

	current = 0;
	decompressSize = 0;

	while (current < CompressSize)
	{
		byte = CompressData[current];
		current++;
		count = CompressData[current];
		current++;

		for (unsigned int i = 0; i < count + 1; i++)
		{
			DecompressData[decompressSize] = byte;
			decompressSize++;
		}
	}

	return decompressSize;
}