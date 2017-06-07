#include "Dialogues.h"
#include <string>
#include <iostream>
#include "Engine.h"
#include"Head.h"

using namespace std;

vector<string> Dialogues::m_Dialogues;

Dialogues::Dialogues()
{
}


Dialogues::~Dialogues()
{
}




bool Dialogues::InitDialogusDate()
{
	if (m_Dialogues.size()!=0)
	{
		return false;
	}

	string idxPath, grpPath;
	idxPath = "Dialogues/talk.idx";
	grpPath = "Dialogues/talk.grp";
	int num;
	FILE *fp = fopen(idxPath.c_str(), "rb");
	if (!fp)
	{
		fprintf(stderr, "Can not open file %s\n", idxPath.c_str());
		return false;
	}
	fseek(fp, 0, SEEK_END);
	int length = ftell(fp);
	fseek(fp, 0, 0);
	for (int i = 0; i<length / 4; i++)
	{
		fread(&num, 4, 1, fp);
		m_idxLen.push_back(num);
		if (i>0)
		{
			fseek(fp, 4, SEEK_CUR);
		}
	}
	fclose(fp);

	fp = fopen(grpPath.c_str(), "rb");
	if (!fp)
	{
		fprintf(stderr, "Can not open file %s\n", grpPath.c_str());
		return false;
	}

	string str;
	for (int j = 0; j<m_idxLen.size(); j++)
	{
		if (j == 0)
		{
			unsigned char *buffer = new unsigned char[m_idxLen[j]];
			fseek(fp, 0, SEEK_SET);
			fread(buffer, m_idxLen[j], 1, fp);
			for (int k = 0; k < m_idxLen[j]; k++)
			{
				buffer[k] = buffer[k] ^ 0xFF;
				if (buffer[k] == 0x2A)
				{
					buffer[k] = 0;
				}
			}
			str = (char*)buffer;
			delete buffer;
			buffer = nullptr;
		}
		else
		{
			unsigned char *buffer = new unsigned char[m_idxLen[j] - m_idxLen[j - 1]];
			fseek(fp, m_idxLen[j - 1], SEEK_SET);
			fread(buffer, m_idxLen[j] - m_idxLen[j - 1], 1, fp);
			for (int k = 0; k < m_idxLen[j] - m_idxLen[j - 1]; k++)
			{
				buffer[k] = buffer[k] ^ 0xFF;
				if (buffer[k]==0x2A)
				{
				buffer[k] = 0;
				}
			}
			str = (char*)buffer;
			delete buffer;
			buffer = nullptr;
		}
		Dialogues::m_Dialogues.push_back(str);
	}
	fclose(fp);

	//fontsName = "fonts/Dialogues.ttf";

	return true;
}

void Dialogues::draw()
{
	
	if (PrinterEffects)
	{
		int length = talkString.length();
		if (strlength < length)
		{
			if (tempSpeed<0)
			{
				tempTalk = talkString.substr(0, strlength);
				strlength+=2;
				tempSpeed = speed;
			}
			tempSpeed--;
		}	
	}
	else
	{
		tempTalk = talkString;
	}
	Engine::getInstance()->drawText(fontsName,GBKToUTF8(tempTalk), 20, 5, 5, 255, BP_ALIGN_LEFT, color);
	
}

void Dialogues::SetFontsName(const string& fontsname)
{
	fontsName = fontsname;
}

void Dialogues::SetFontsColor(SDL_Color &Color)
{
	color = Color;
}

void Dialogues::SetDialoguesEffect(bool b)
{
	if (PrinterEffects)
	{
	}
	else
	{
		PrinterEffects = b;
	}

}

void Dialogues::SetDialoguesNum(int num)
{
	talkString = m_Dialogues.at(num);
}

void Dialogues::SetDialoguesSpeed(int sp)
{
	speed = sp;
	tempSpeed = speed;
}