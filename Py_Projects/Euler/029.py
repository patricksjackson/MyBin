def Euler_29():
	'''
		For 2<=a,b<=100, how many unique terms are generated by a^b?
	'''
	full_list = []
	for a in range(2,101):
		tmplist = [a**b for b in range(2,101)]
		for x in tmplist:
			if x not in full_list:
				full_list.append(x)
	return len(full_list)

if __name__ == '__main__':
	print(Euler_29())