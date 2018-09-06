'''
NAME: Jack Li, Kevin Zhang
EMAIL: EMAIL: jackli2014@gmail.com, kevin.zhang.13499@gmail.com
UID: 604754714, 104939334
'''

#!/usr/bin/python

import sys
import csv
from sets import Set

free_blocks = set()
allocated_inodes = set() 
free_inodes = set()
references = dict()
global exit_status


# for finding duplicate blocks
class BlockInfo:
    def __init__(self, inode, offset, level):
        self.inode = inode
        self.offset = offset
        self.level = level

# superblock class for global data
class SuperBlock:
    def __init__(self, properties):
        self.blocks_count = int(properties[1])
        self.inodes_count = int(properties[2])
        self.block_size = int(properties[3])
        self.inode_size = int(properties[4])
        self.blocks_per_group = int(properties[5])
        self.inodes_per_group = int(properties[6])
        self.first_nonreserved_inode = int(properties[7])

class Inode:
    def __init__(self, properties):
        self.inode_num = int(properties[1])
        self.type = properties[2]
        self.mode = properties[3]
        self.link_count = int(properties[6])
        self.addresses = []
        for address in properties[12:27]:
            addresses = int(address)
            self.addresses.append(address)


class Dirent:
    def __init__(self, properties):
        self.name = properties[6]
        self.parent_inode = int(properties[1])
        self.logical_byte_offset = int(properties[2])
        self.file_num = int(properties[3])

# check valid pointers in I-nodes, direct blocks, and indirect blocks 
def block_scan(properties, superblock):
    inode = int(properties[1])
    allocated_inodes.add(inode)
    if properties[0] == "INODE":
        # don't analyze symbolic links less than or equal to 60 bytes long
        if properties[2] == "s" and int(properties[10]) <= 60:
            return
        # direct blocks
        offset = 0
        for block in properties[12:24]:
            block_address = int(block)
            if block_address:
                if block_address >= superblock.blocks_count or block_address < 0:
                    print "INVALID BLOCK {} IN INODE {} AT OFFSET {}".format(block_address, inode, offset)
                    exit_status = 2
                elif block_address > 0 and block_address < 8:
                    print "RESERVED BLOCK {} IN INODE {} AT OFFSET {}".format(block_address, inode, offset)
                    exit_status = 2
                else:
                    block_info = BlockInfo(inode, offset, 0)
                    if block_address not in references:
                        references[block_address] = [ block_info ]
                    else:
                        references[block_address].append(block_info)
            offset = offset + 1

        # single indirect block
        block_address = int(properties[24])
        if block_address:
            if block_address >= superblock.blocks_count or block_address < 0:
                print "INVALID INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(block_address, inode, 12)
                exit_status = 2
            elif block_address > 0 and block_address < 8:
                print "RESERVED INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(block_address, inode, 12)
                exit_status = 2
            else:
                block_info = BlockInfo(inode, 12, 1)
                if block_address not in references:
                    references[block_address] = [ block_info ]
                else:
                    references[block_address].append(block_info) 
               
        # double indirect block
        block_address = int(properties[25])
        if block_address:
            if block_address >= superblock.blocks_count or block_address < 0:
                print "INVALID DOUBLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(block_address, inode, 268)
                exit_status = 2
            elif block_address > 0 and block_address < 8:
                print "RESERVED DOUBLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(block_address, inode, 268)
                exit_status = 2
            else:
                block_info = BlockInfo(inode, 268, 2)
                if block_address not in references:
                    references[block_address] = [ block_info ]
                else:
                    references[block_address].append(block_info)

        # triple indirect block
        block_address = int(properties[26])
        if block_address:
            if block_address >= superblock.blocks_count or block_address < 0:
                print "INVALID TRIPLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(block_address, int(properties[1]),
                    65804)
                exit_status = 2
            elif block_address > 0 and block_address < 8:
                print "RESERVED TRIPLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(block_address, int(properties[1]),
                    65804)
                exit_status = 2
            else:
                block_info = BlockInfo(inode, 65804, 3)
                if block_address not in references:
                    references[block_address] = [ block_info ]
                else:
                    references[block_address].append(block_info)
    else:
        level = int(properties[2])
        block_address = int(properties[5])
        offset = int(properties[3])

        if block_address:
            if level == 1:
                strlevel = "INDIRECT"
            elif level == 2:
                strlevel = "DOUBLE INDIRECT"
            else:
                strlevel = "TRIPLE INDIRECT"

            if block_address >= superblock.blocks_count or block_address < 0:
                print "INVALID {} BLOCK {} IN INODE {} AT OFFSET {}".format(strlevel, block_address, inode, offset)
                exit_status = 2
            elif block_address > 0 and block_address < 8:
                print "RESERVED {} BLOCK {} IN INODE {} AT OFFSET {}".format(strlevel, block_address, inode, offset)
                exit_status = 2
            else:
                block_info = BlockInfo(inode, offset, level)
                if block_address not in references:
                    references[block_address] = [ block_info ]
                else:
                    references[block_address].append(block_info)
            

def directory_scan(inodes, dirents, superblock):
    parents = dict();
    parents[2] = 2; #parent of root directory is root
    
    for dirent in dirents:
        if dirent.file_num <= superblock.inodes_count:
            if dirent.name != "'..'" and dirent.name != "'.'":
                parents[dirent.file_num] = dirent.parent_inode
    for inode in inodes:
        numReferences = 0;
        for dirent in dirents:
            if dirent.file_num == inode.inode_num:
                numReferences+=1
        if numReferences != inode.link_count:
            print "INODE {} HAS {} LINKS BUT LINKCOUNT IS {}".format(inode.inode_num, numReferences, inode.link_count)
            exit_status = 2

    for dirent in dirents:
        inodeNum = dirent.file_num
        parentNum = dirent.parent_inode
        # 1 is parentnum 3 is inode num
        if dirent.name == "'.'" and inodeNum != parentNum:
            print "DIRECTORY INODE {} NAME {} LINK TO INODE {} SHOULD BE {}".format(
                                                                                    parentNum,
                                                                                    dirent.name,
                                                                                    inodeNum,
                                                                                    parentNum)
            exit_status = 2
        if dirent.name == "'..'" and inodeNum != parents[parentNum]:
            print "DIRECTORY INODE {} NAME {} LINK TO INODE {} SHOULD BE {}".format(
                                                                                    parentNum,
                                                                                    dirent.name,
                                                                                    dirent.file_num,
                                                                                    parents[parentNum])
            exit_status = 2
    for dirent in dirents:
        cur_inode = None
        for inode in inodes:
            if inode.inode_num == dirent.file_num:
                cur_inode = inode
                break
        if cur_inode == None and dirent.file_num > 0 and dirent.file_num < superblock.inodes_count:
            print "DIRECTORY INODE {} NAME {} UNALLOCATED INODE {}".format(dirent.parent_inode, dirent.name, dirent.file_num)
            exit_status = 2
        elif cur_inode == None or (cur_inode.inode_num < 1 or cur_inode.inode_num > superblock.inodes_count):
            print "DIRECTORY INODE {} NAME {} INVALID INODE {}".format(dirent.parent_inode, dirent.name, dirent.file_num)
            exit_status = 2
        else:
            allocated_inodes.add(cur_inode)


def main():
    # pre-processing
    if len(sys.argv) != 2:
        sys.stderr.write("Wrong number of arguments\n")
        sys.exit(1)
    
    exit_status = 0
    superblock = None
    inodes = []
    dirents = []
    try:
        with open(sys.argv[1], 'rb') as f:
            reader = csv.reader(f, delimiter=',')
            for line in reader:
                if line[0] == "INODE" or line[0] == "INDIRECT":                                     
                    block_scan(line, superblock)
                    if line[0] == "INODE":
                        inodes.append(Inode(line))
                elif line[0] == "BFREE":
                    free_blocks.add(int(line[1]))
                elif line[0] == "IFREE":
                    free_inodes.add(int(line[1]))
                elif line[0] == "DIRENT":
                    dirents.append(Dirent(line))
                elif line[0] == "SUPERBLOCK":
                    superblock = SuperBlock(line)
    except IOError:
        sys.stderr.write("Could not open file\n")
        sys.exit(1)

    directory_scan(inodes, dirents, superblock)

    # check free block list for unreferenced and allocated inconsistencies
    for block in range(superblock.blocks_count):
        if block not in references and block not in free_blocks and block >= 8:
            print "UNREFERENCED BLOCK {}".format(block)
            exit_status = 2
        elif block in references and block in free_blocks:
            print "ALLOCATED BLOCK {} ON FREELIST".format(block)
            exit_status = 2

    # check allocated inodes list for allocated inconsistencies
    for inode in allocated_inodes:
        if inode in free_inodes:
            print "ALLOCATED INODE {} ON FREELIST".format(inode)
            exit_status = 2

    # check free inodes list for unreferenced inconsistencies
    for inode in range(superblock.first_nonreserved_inode, superblock.inodes_count):
        if inode not in allocated_inodes and inode not in free_inodes:
            print "UNALLOCATED INODE {} NOT ON FREELIST".format(inode)
            exit_status = 2

    # check for duplicates
    for block, block_info_array in references.items():
        if len(block_info_array) > 1:
            exit_status = 2
            for block_info in block_info_array:
                if block_info.level == 0:
                    print "DUPLICATE BLOCK {} IN INODE {} AT OFFSET {}".format(block, block_info.inode,
                        block_info.offset)
                elif block_info.level == 1:
                    print "DUPLICATE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(block, block_info.inode,
                        block_info.offset)
                elif block_info.level == 2:
                    print "DUPLICATE DOUBLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(block, block_info.inode,
                        block_info.offset)
                else:
                    print "DUPLICATE TRIPLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(block, block_info.inode,
                        block_info.offset)
    
    sys.exit(exit_status)


if __name__ == "__main__":
    main()
