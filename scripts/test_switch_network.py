"""Run the production socket configuration wrappers with a host BSD fixture."""
from pathlib import Path
import argparse
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def method(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


FIXTURE = r'''
#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
typedef unsigned socklen_t;
static atomic_int largest_receive_buffer;
enum { SOL_SOCKET=1, SO_RCVBUF=2 };
static int set_result=0, actual=0, query_result=0, calls=0;
int __real_setsockopt(int s,int l,int o,const void* v,socklen_t n) {
    assert(s==7&&v&&n==sizeof(int));++calls;return set_result;
}
int getsockopt(int s,int l,int o,void* v,socklen_t* n) {
    assert(s==7&&l==SOL_SOCKET&&o==SO_RCVBUF);*(int*)v=actual;return query_result;
}
'''
SCENARIOS = r'''
int main(void) {
    int requested=3*1024*1024;
    actual=2*1024*1024;assert(__wrap_setsockopt(7,SOL_SOCKET,SO_RCVBUF,&requested,sizeof(requested))==0);
    assert(moonlightSocketReceiveBufferBytes()==actual);
    actual=65536;__wrap_setsockopt(7,SOL_SOCKET,SO_RCVBUF,&requested,sizeof(requested));
    assert(moonlightSocketReceiveBufferBytes()==2*1024*1024);
    actual=4*1024*1024;set_result=-1;
    assert(__wrap_setsockopt(7,SOL_SOCKET,SO_RCVBUF,&requested,sizeof(requested))==-1);
    assert(moonlightSocketReceiveBufferBytes()==2*1024*1024);
    set_result=0;query_result=-1;__wrap_setsockopt(7,SOL_SOCKET,SO_RCVBUF,&requested,sizeof(requested));
    assert(moonlightSocketReceiveBufferBytes()==2*1024*1024&&calls==4);
    puts("PASS: BSD return values and actual receive-buffer reporting");
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cc', default='cc')
    args = parser.parse_args()
    source = (ROOT/'app/src/switch/wrapper.c').read_text()
    functions = '\n'.join(method(source, signature) for signature in (
        'int __wrap_setsockopt(', 'int moonlightSocketReceiveBufferBytes('))
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)/'network.c'
        binary = Path(directory)/'network.exe'
        path.write_text(FIXTURE+functions+SCENARIOS)
        subprocess.run([args.cc,'-std=c11','-Wall','-Wextra','-Werror',
                        '-Wno-unused-parameter',str(path),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)


if __name__ == '__main__':
    main()
