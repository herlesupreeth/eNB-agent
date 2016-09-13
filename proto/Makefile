# Copyright (c) 2016 Kewin Rausch <kewin.rausch@create-net.org>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#
# Makefile to compile the Empower Agent protocol buffers.
#

CC=gcc
PCC=protoc-c

INCLDIR=/usr/include/emage
INSTDIR=/usr/lib

PROTO-OUT=./pb

all:
	make proto
	make protolib
	
protolib:
	$(CC) -I./pb -c -fpic ./pb/*.pb-c.c
	$(CC) -shared -o libemproto.so *.pb-c.o  

proto:
	mkdir -p $(PROTO-OUT)
	$(PCC) ./*.proto --c_out=$(PROTO-OUT)

clean:
	rm -rf $(PROTO-OUT)
	rm -f ./*.o
	rm -f ./*.a
	rm -f ./*.so
	
install:
	cp ./libemproto.so $(INSTDIR)
	mkdir -p $(INCLDIR)/pb/
	cp -r ./pb/* $(INCLDIR)/pb/
	
uninstall:
	rm $(INSTDIR)/libemproto.so
	rm -r $(INCLDIR)/pb
	