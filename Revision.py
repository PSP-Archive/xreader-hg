#!/usr/bin/python

import sys, os, subprocess, StringIO, re

REVISON_PATH="./Revision.h"

def get_revision():
	fp = open(REVISON_PATH)
	data = fp.read()
	fp.close()
	return data

def update_revision(data):
	fp = open(REVISON_PATH, "w")
	fp.write(data)
	fp.close()

def main():
	output = subprocess.Popen("hg summary", shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE).communicate()[0]
	sio = StringIO.StringIO(output)
	rev_id = "Unknown"

	while 1:
		line = sio.readline()
		if len(line) <= 0:
			break

		m = re.search("parent:[ \t]*(\d+)", line, re.I)

		if m:
			rev_id = m.group(1)
			break

	new_rev = ("#define REVISION \"hg-%s\"\n"%(rev_id))

	sts = None

	try:
		sts = os.stat(REVISON_PATH)
	except OSError:
		pass

	if sts:
		old_rev = get_revision()

		if old_rev != new_rev:
			print("%s: Update Revision %s..." % (sys.argv[0], rev_id))
			update_revision(new_rev)
	else:
		print("%s: Write Revision %s..." % (sys.argv[0], rev_id))
		update_revision(new_rev)

if __name__ == "__main__":
	main()
