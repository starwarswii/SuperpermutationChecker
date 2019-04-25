import sys
import os
import subprocess

def run(string):

	print(string)
	
	rc = os.system(string)
	if rc != 0:
		print("exiting")
		sys.exit()
		
def runWithOutput(string, exitOnError=True):

	#print(string)

	commands = string.split(" ")
	
	try:
		result = subprocess.check_output(commands, stderr=subprocess.STDOUT)

	except:
		if exitOnError:
			print("non-zero exit code, exiting")
			sys.exit()
		
	#print(result)
	
	return result
	
def testSerial(num, filename, negativeTest):
	filesize = os.stat(filename).st_size
	
	result = runWithOutput("./a.out "+str(num)+" "+filename)
	
	#we need the parenthesies, otherwise it evaluates to [(negativeTest != "good") in result]
	#which becomes [False in result]
	correct = negativeTest != ("good!" in result)
	
	if correct:
		print "file", filename, "with serial version is correct"
	else:
		print result
		print "incorrect test found!"
		print "using file", filename, "with serial version"
		sys.exit()
		
def testParallel(num, filename, negativeTest, limit=15):
	filesize = os.stat(filename).st_size
	
	for i in range(1, min(filesize, limit)+1):
		result = runWithOutput("mpirun -n "+str(i)+" ./a.out "+str(num)+" "+filename)
		
		#we need the parenthesies, otherwise it evaluates to [(negativeTest != "good") in result]
		#which becomes [False in result]
		correct = negativeTest != ("good!" in result)
		
		if correct:
			print "file", filename, "with", i, "ranks is correct"
		else:
			print result
			print "incorrect test found!"
			print "using file", filename, "and numRanks", i
			sys.exit()
			
def testHyperparallel(num, filename, negativeTest, rankLimit=15, threadLimit=10):
	filesize = os.stat(filename).st_size
	
	for i in range(1, min(filesize, rankLimit)+1):
		
	
		for j in range(1, min(filesize//i, threadLimit)+1):
			result = runWithOutput("mpirun -n "+str(i)+" ./a.out "+str(num)+" "+filename+" "+str(j), False)
			
			if "error: threads per rank" in result:
				print result
				print "found highest thread count", j, "for", i, "ranks. skipping to next rank value\n"
				continue
			
			#we need the parenthesies, otherwise it evaluates to [(negativeTest != "good") in result]
			#which becomes [False in result]
			correct = negativeTest != ("good!" in result)
			
			if correct:
				print "file", filename, "with", i, "ranks and", j, "threads is correct"
			else:
				print result
				print "incorrect test found!"
				print "using file", filename, " numRanks", i, "numThreads", j
				sys.exit()

######################################## BEGIN ########################################


	
if len(sys.argv) == 1:
	
	run("mpicc -Wall perm-parallel.c")
	
	valids = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]
	invalids = [4, 5, 7, 8, 9, 10, 11]
	
	for num in valids:
		filename = "tests/valid"+str(num)+".in"
		testParallel(num, filename, False)
		print ""
		
	for num in invalids:
		filename = "tests/invalid"+str(num)+".in"
		testParallel(num, filename, True)
		print ""
		
	run("mpicc -Wall perm-serial.c")
		
	for num in valids:
		filename = "tests/valid"+str(num)+".in"
		testSerial(num, filename, False)
		print ""
		
	for num in invalids:
		filename = "tests/invalid"+str(num)+".in"
		testSerial(num, filename, True)
		print ""
		
	run("mpicc -Wall perm-hyperparallel.c -lpthread")
		
	num = 5
	testHyperparallel(num, "tests/valid"+str(num)+".in", False, 10)
	print ""
	testHyperparallel(num, "tests/invalid"+str(num)+".in", True, 10)
	
else:

	run("mpicc -Wall perm-parallel.c")

	type = sys.argv[1]
	num = sys.argv[2]
		
	filename = "tests/"+type+num+".in"
	negativeTest = type == "invalid"

	testParallel(num, filename, negativeTest)
