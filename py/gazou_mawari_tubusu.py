import sys
import cv2
from collections import deque
import copy

if len(sys.argv) < 4:
	sys.stderr.write("python gazou_mawari_tubusu.py input_img mask_img output_img\n")
	sys.exit(1)

inputImgName = sys.argv[1]
maskImgName = sys.argv[2]
outputImgName = sys.argv[3]

img = cv2.imread(inputImgName)
maskImg = cv2.imread(maskImgName, cv2.IMREAD_GRAYSCALE)

if img.shape[0:2] != maskImg.shape:
	sys.stderr.write("size of input and mask image mismatch\n")
	sys.exit(1)

width = img.shape[1]
height = img.shape[0]
validFlag = [[element >= 128 for element in row] for row in maskImg]

def isValid(y, x):
	if y < 0 or len(validFlag) <= y or x < 0 or len(validFlag[y]) <= x:
		return False
	return validFlag[y][x]

visitedFlag = copy.deepcopy(validFlag)
workQueue = deque()

def checkAndAppendWork(y, x):
	if 0 <= y and y < len(visitedFlag) and 0 <= x and x < len(visitedFlag[y]):
		if not visitedFlag[y][x]:
			validCount = 0
			for dy in [-1, 0, 1]:
				for dx in [-1, 0, 1]:
					if (dx != 0 or dy != 0) and isValid(y + dy, x + dx):
						validCount += 1
			if validCount >= 3:
				workQueue.append((y, x))
				visitedFlag[y][x] = True

for y in range(height):
	for x in range(width):
		checkAndAppendWork(y, x)

sample = img[0][0]
if hasattr(sample, "__iter__"):
	nElem = len(sample)
else:
	nElem = 0

while len(workQueue) > 0:
	y, x = workQueue.popleft()
	if nElem > 0:
		average = [0 for i in range(nElem)]
	else:
		average = 0
	count = 0
	for dy in [-1, 0, 1]:
		for dx in [-1, 0, 1]:
			if isValid(y + dy, x + dx):
				pxData = img[y + dy][x + dx]
				if nElem > 0:
					for i in range(nElem):
						average[i] += pxData[i]
				else:
					average += pxData
				count += 1
	if nElem > 0:
		for i in range(nElem):
			average[i] /= count
	else:
		average /= count
	img[y][x] = average
	validFlag[y][x] = True
	for dy in [-1, 0, 1]:
		for dx in [-1, 0, 1]:
			checkAndAppendWork(y + dy, x + dx)

allValid = True
for row in validFlag:
	for valid in row:
		if not valid:
			allValid = False

if not allValid:
	sys.err.write("warning: there are some invalid pixels left!")

cv2.imwrite(outputImgName, img)
