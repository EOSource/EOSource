#ifndef FWD_DIALOG_HPP_INCLUDED
#define FWD_DIALOG_HPP_INCLUDED

class Dialog;

enum DialogEntry : unsigned char
{
	DIALOG_TEXT = 1,
	DIALOG_LINK = 2
};

enum DialogReply : unsigned char
{
	DIALOG_REPLY_OK = 1,
	DIALOG_REPLY_LINK = 2,
};

#endif
