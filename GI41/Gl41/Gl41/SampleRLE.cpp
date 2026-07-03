#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>


void Compress(const char* FileName, FILE* CompressFile);
void Decompress(FILE* CompressFile);

unsigned int Encode(unsigned char* OriginalData, unsigned int OriginalSize, unsigned char* CompressData);
unsigned int Decode(unsigned char* CompressData, unsigned int CompressSize, unsigned char* DecompressData);




int main()
{
	printf("1:圧縮　2:展開\n");

	int mode;
	scanf("%d", &mode);

	if (mode == 1)
	{
		//圧縮

		FILE* compressfile;
		compressfile = fopen("compress.dat", "wb");

		Compress("data/explosion.wav", compressfile);
		Compress("data/hal.bmp", compressfile);
		Compress("data/TeraPad.exe", compressfile);
		Compress("data/wagahaiwa_nekodearu.txt", compressfile);
		Compress("data/yuuki_256.bmp", compressfile);

		fclose(compressfile);
	}
	else if(mode == 2)
	{
		//復号
		
		FILE* compressfile;
		compressfile = fopen("compress.dat", "rb");

		Decompress(compressfile);

		fclose(compressfile);

		// 展開が終わったら圧縮ファイルを削除する
		if (remove("compress.dat") == 0)
		{
			printf("compress.dat を削除しました。\n");
		}
		else
		{
			printf("compress.dat を削除できませんでした。\n");
		}
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

	// 圧縮後、元データを削除する
	if (remove(FileName) == 0)
	{
		printf("%s を削除しました。\n", FileName);
	}
	else
	{
		printf("%s を削除できませんでした。\n", FileName);
	}
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
	//ランレングスで圧縮
	unsigned int i = 0;
	unsigned int compressSize = 0;

	while (i < OriginalSize)
	{
		unsigned char current = OriginalData[i];

		// 連続数を数える
		unsigned int count = 1;
		while (i + count < OriginalSize &&
			OriginalData[i + count] == current &&
			count < 255)
		{
			count++;
		}

		// 「圧縮する価値があるか」で分岐
		if (count >= 3)
		{
			// RLE圧縮
			CompressData[compressSize++] = 1;      // type = RLE
			CompressData[compressSize++] = current;
			CompressData[compressSize++] = count;
		}
		else
		{
			// RAWモード（圧縮しない）
			unsigned int start = i;
			unsigned int rawCount = 0;

			while (i < OriginalSize)
			{
				unsigned char next = OriginalData[i];

				// ここでRLEに切り替え条件
				if (i + 2 < OriginalSize)
				{
					unsigned char n1 = OriginalData[i + 1];
					unsigned char n2 = OriginalData[i + 2];
					if (next == n1 && n1 == n2)
						break;
				}

				i++;
				rawCount++;

				if (rawCount >= 255)
					break;
			}

			CompressData[compressSize++] = 0; // type = RAW
			CompressData[compressSize++] = (unsigned char)rawCount;

			for (unsigned int k = 0; k < rawCount; k++)
			{
				CompressData[compressSize++] = OriginalData[start + k];
			}

			continue;
		}

		i += count;
	}

	return compressSize;
}





unsigned int Decode(unsigned char* CompressData, unsigned int CompressSize, unsigned char* DecompressData)
{
	//ランレングスで展開
	unsigned int i = 0;
	unsigned int out = 0;

	while (i < CompressSize)
	{
		unsigned char type = CompressData[i++];

		if (type == 1)
		{
			// RLE
			unsigned char byte = CompressData[i++];
			unsigned char count = CompressData[i++];

			for (int j = 0; j < count; j++)
				DecompressData[out++] = byte;
		}
		else
		{
			// RAW
			unsigned char count = CompressData[i++];

			for (int j = 0; j < count; j++)
				DecompressData[out++] = CompressData[i++];
		}
	}

	return out;
}