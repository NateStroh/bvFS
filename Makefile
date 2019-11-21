CXX=g++ -std=c++17 -g -w -fmax-errors=1 -m32

bvfs_tester: bvfs_tester.cpp bvfs.h
	${CXX} bvfs_tester.cpp -o bvfs_tester

run: bvfs_tester
	./bvfs_tester

run12: bvfs_tester
	./bvfs_tester 12


clean:
	@echo "Cleaning..."
	rm -f bvfs_tester
