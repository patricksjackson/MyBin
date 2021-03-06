import sys,re,string

#def playInfo( quarter, playid, posession, player1, player2, playtype, yards, result ):
#

#def processPlay( list, index):
    
    
def findPlayer( list, index):
    search=re.search('playerId=[0-9]+',list[index-1])
    if search!=None:
        player=re.search('([A-Z][a-z]* *)+(?=( <)|<)',list[index]).group(0)
        return player
    return None

file = open(sys.argv[1])
team=['ABC', 'ABC']
fullname=['ABC','ABC']
fnstate=0
# Getting team names first makes subsequent searches easier
for line in file:
    # Search for team
    search = re.search('[A-Z]{1,3}(?= at [A-Z]{1,3})',line)
    if search!=None:
        team[0]=search.group(0)
        search = re.search('(?<=' + team[0] + ' at )[A-Z]{1,3}',line)
        if search!=None:
            team[1] = search.group(0)
    # Search for full team name
    words=string.split(line,'>')
    
    for w in words:
        if fnstate<2:
            search=re.match(' *([A-Z][a-z]* )+(?=[0-9]{1,3}[, ]*<)',w)
            if search!=None:
                fullname[fnstate]=string.strip(search.group(0))
                fnstate+=1

print fullname
file.seek(0) # Seek back to beginning of file

record=['ABC','ABC']
# Search for team record
for line in file:
    search=re.search('(?<='+team[0]+'</A> \()[0-9]{1,2}-[0-9]{1,2}\).*',line)
    if search!=None:
        # Found the string of info we're looking for
        record[0]=re.search('[0-9]{1,2}-[0-9]{1,2}',search.group(0)).group(0)
        w=string.split(search.group(0),'>')
        state='begin'
        playstate=0
        for i in range(1,len(w)):
            if state=='begin':
                quarter='1'
                if re.match(' \([0-9]{1,2}-[0-9]{1,2}',w[i])!=None:
                    record[1]=re.search('[0-9]{1,2}-[0-9]{1,2}',w[i]).group(0)
                    state='quarter'
            elif state=='quarter':
                if re.search(quarter+'[a-zA-Z]{2} (Quarter)|(QUARTER)',w[i])!=None:
                    state='drive'
            elif state=='drive':
                search=re.search(fullname[0]+' at [0-9]{1,2}:[0-9]{2}',w[i])
                if search!=None:
                    currentteam=0
                    state='play'
                    continue
                search=re.search(fullname[1]+' at [0-9]{1,2}:[0-9]{2}',w[i])
                if search!=None:
                    currentteam=1
                    state='play'
                    continue
            elif state=='play':
                # Determine play position
                if playstate==0:
                    search=re.search('[1-4][a-zA-Z]{2} and [0-9]+ at *',w[i])
                    if search!=None:
                        down=search.group(0)[0]
                        yardstogo=re.search('(?<=[1-4][a-zA-Z]{2} and )[0-9]+(?= at )',search.group(0)).group(0)
                        if re.search('(?<= at )'+team[0]+' [0-9]+',w[i])!=None:
                            positionteam=team[0];
                            positionyard=re.search('(?<= at '+team[0]+' )[0-9]+',w[i]).group(0)
                            playstate+=1
                        elif re.search('(?<= at )'+team[1]+' [0-9]+',w[i])!=None:
                            positionteam=team[1];
                            positionyard=re.search('(?<= at '+team[1]+' )[0-9]+',w[i]).group(0)
                            playstate+=1
                if playstate==1:
                    player=findPlayer(w,i)
                    if player!=None:
                        #playstate+=1
                        print player
                if playstate==2:
                    if re.search('rush for',w[i])!=None:
                        #rushdistance=re.search('(?<=rush for )[0-9]+(?= yards)',w[i]).group(0)
                    #other info
                        state='play'
                
    
print record

#    search = re.search('Georgia Tech (?!Yellow)',line)
#    if search!=None:
#        print line[]

    
    

if team[0]=='ABC' or team[1]=='ABC':
    print 'Team names not found'
    sys.exit()

print team[0] + ' played at ' + team[1]

