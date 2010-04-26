/*
File: printf.c

Copyright (C) 2004,2008  Kustaa Nyholm

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

copied from http://www.sparetimelabs.com/printfrevisited/index.html
modified by Philipp Scholl <scholl@teco.edu>

*/

#include <AppHardwareApi.h>
#include "conf.h"
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

static char*  bf, buf[12], uc, zs;
static unsigned int num;

static void out(char c) {
  *bf++ = c;
}

static void outDgt(char dgt) {
  out(dgt+(dgt<10 ? '0' : (uc ? 'A' : 'a')-10));
  zs=1;
}

static void divOut(unsigned int div) {
  unsigned char dgt=0;
  num &= 0xffff; // just for testing the code  with 32 bit ints
  while (num>=div) {
    num -= div;
    dgt++;
  }
  if (zs || dgt>0) outDgt(dgt);
}


int vsnprintf(char *str, size_t n, const char *fmt, va_list va)
{
  char ch, *p, *str_orig = str;

  while ((ch=*fmt++) && str-str_orig < n) {
    if (ch!='%') {
      *str++ = ch;
    } else {
      char lz=0;
      char w=0;
      ch=*(fmt++);
      if (ch=='0') {
        ch=*(fmt++);
        lz=1;
      }
      if (ch>='0' && ch<='9') {
        w=0;
        while (ch>='0' && ch<='9') {
          w=(((w<<2)+w)<<1)+ch-'0';
          ch=*fmt++;
        }
      }
      bf=buf;
      p=bf;
      zs=0;
      switch (ch) {
        case 0:
          goto abort;
        case 'u':
        case 'd':
          num=va_arg(va, unsigned int);
          if (ch=='d' && (int)num<0) {
            num = -(int)num;
            out('-');
          }
          divOut(10000);
          divOut(1000);
          divOut(100);
          divOut(10);
          outDgt(num);
          break;
        case 'x':
        case 'X':
          uc= ch=='X';
          num=va_arg(va, unsigned int);
          divOut(0x1000);
          divOut(0x100);
          divOut(0x10);
          outDgt(num);
          break;
        case 'c':
          out((char) (va_arg(va, int)));
          break;
        case 's':
          p=va_arg(va, char*);
          break;
        case '%':
          out('%');
        default:
          break;
      }
      *bf=0;
      bf=p;

      while (*bf++ && w > 0)
        w--;
      while (w-- > 0)
        if (str-str_orig < n)
          *str++ = lz ? '0' : ' ';
        else
          goto abort;
      while ((ch= *p++))
        if (str-str_orig < n)
          *str++ = ch;
        else
          goto abort;
    }
  }

abort:

  return str - str_orig;
}

int sprintf(char *str, const char *fmt, ...)
{
  int m;
  va_list va;
  va_start(va,fmt);
  m = vsnprintf(str, 0xffffffff, fmt, va);
  va_end(va);
  return m;
}

int snprintf(char *str, size_t n, const char *fmt, ...)
{
  int m;
  va_list va;
  va_start(va,fmt);
  m = vsnprintf(str, n, fmt, va);
  va_end(va);
  return m;
}

int printf(const char *fmt, ...)
{
  int m;
  char str[256];
  va_list va;
  va_start(va,fmt);
  m = vsnprintf(str, sizeof(str)-1, fmt, va);
  va_end(va);
  str[m+1] = '\0';
  uart_write(UART, str, m);
  return m;
}

int puts(const char *s)
{
  return uart_write(UART, s, strlen(s));
}

//int putchar(int c)
//{
//  return uart_write(UART, c, 1);
//}
