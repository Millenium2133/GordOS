// Minimal userland ctype for GordOS user programs. Pure computation,
// no syscalls needed. TCC's preprocessor/tokenizer leans on these for
// whitespace/identifier/number scanning.
#ifndef CTYPE_H
#define CTYPE_H

int isalpha(int c);
int isdigit(int c);
int isspace(int c);
int isxdigit(int c);
int isalnum(int c);
int toupper(int c);
int tolower(int c);

#endif // CTYPE_H