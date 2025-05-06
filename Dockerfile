FROM python:3 AS build

WORKDIR /build

RUN apt-get -y update && apt-get -y install git g++ autoconf-archive make libtool gfortran tar gawk

RUN wget http://www.openfst.org/twiki/pub/FST/FstDownload/openfst-1.8.4.tar.gz && \
	tar -xvzf openfst-1.8.4.tar.gz && \
	cd openfst-1.8.4 && \
	./configure --enable-static --enable-shared --enable-far --enable-ngram-fsts --enable-lookahead-fsts --with-pic && \
	make -j $(nproc) && \
	make install && \
	ldconfig

RUN git clone https://github.com/mitlm/mitlm && \
	cd mitlm && \
	autoreconf -i && \
	./configure && \
	make -j $(nproc) && \
	make install

WORKDIR /build/phonetisaurus

COPY . ./

RUN pip3 install pybindgen setuptools

RUN ./configure --enable-python && \
	make -j $(nproc) && \
	make install 

FROM python:3-slim

RUN apt-get -y update && apt-get -y install gfortran && apt-get -y clean && apt-get -y autoclean && \
	pip3 install setuptools

WORKDIR /setup

COPY --from=build /build/phonetisaurus/python ./
COPY --from=build /build/phonetisaurus/.libs/Phonetisaurus.so ./

RUN python setup.py install

COPY --from=build /usr/local/lib/fst /usr/local/lib/fst
COPY --from=build /usr/local/lib/libfst*so*0 /usr/local/lib/
COPY --from=build /usr/local/bin/phonetisaurus* /usr/local/bin/
COPY --from=build /build/phonetisaurus/src/scripts/* /usr/local/bin/
COPY --from=build /usr/local/bin/rnnlm /usr/local/bin/
COPY --from=build /usr/local/bin/estimate-ngram /usr/local/bin/
COPY --from=build /usr/local/lib/libmitlm.so.1.0.0 /usr/local/lib

RUN ldconfig

WORKDIR /work

ENTRYPOINT [ "/bin/bash" , "-c" ]
