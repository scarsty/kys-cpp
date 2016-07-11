#include "error.h"

int Error::_count=0;
vector<ErrorNote> Error::_notes(1);

Error::Error()
{

}
void Error::setError(int code, std::string note)
{
	_count++;
	ErrorNote a;
	a.code = code;
	a.note = note;
	_notes.push_back(a);
}