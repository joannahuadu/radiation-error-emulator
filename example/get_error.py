import argparse
import random
parser = argparse.ArgumentParser(description='get offset')
parser.add_argument(
    '--total_bit',
    type=int,
    help='total flipped bits')

args = parser.parse_args()
print(args.total_bit)

total_bit = args.total_bit
print("total_bit: ", total_bit)
mbu_2bit_max = int(total_bit*0.12)
mbu_2bit_min = int(total_bit*0.02)
mbu_3bit_max = int(total_bit*0.02)
mbu_nbit_max = int(total_bit*0.01)
counter = 0

with open('error_counts_'+str(total_bit)+'.txt', 'w') as f:
    while True:
        error_count = {}
        mbu_2bit = random.randint(mbu_2bit_min, mbu_2bit_max)
        mbu_3bit = random.randint(0, mbu_3bit_max)
        mbu_nbit = random.randint(0, mbu_nbit_max)
        c_n = 0
        flip_bit = 2*mbu_2bit + 3*mbu_3bit
        n_bit = []
        while c_n < mbu_nbit:
            error = random.randint(5,7)
            n_bit.append(error)
            c_n += 1
            flip_bit += error
        
        if total_bit-flip_bit<0:
            print("out of total bits!!!")
            continue
        seu_bit = (total_bit - flip_bit) if (total_bit - flip_bit) >= 0 else 0
        assert(seu_bit + flip_bit == total_bit)
        
        for _ in range(mbu_2bit):
            if 2 in error_count:
                error_count[2] += 1
            else:
                error_count[2] = 1
    
        for _ in range(mbu_3bit):
            if 3 in error_count:
                error_count[3] += 1
            else:
                error_count[3] = 1
        
        for error in n_bit:
            if error in error_count:
                error_count[error] += 1
            else:
                error_count[error] = 1
        
        for _ in range(seu_bit):
            if 1 in error_count:
                error_count[1] += 1
            else:
                error_count[1] = 1


        for error, count in error_count.items():
            # f.write(f"Error: {error}, Count: {count}\n")
            f.write(f"{error}:{count} ")
        f.write(f"\n")
        counter+=1
        print("success{0}:[2:{1}, 3:{2}, N:{3}, 1:{4}]!".format(counter, mbu_2bit, mbu_3bit, mbu_nbit, total_bit-flip_bit))

        if counter == 1000:
            break
    
print("finish!!")
