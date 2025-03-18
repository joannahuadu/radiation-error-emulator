import subprocess
import re
import time


def run_resnet50(times, t):
    accuracies = []
    total_times = []
    total_accumulated_time = 0
    total_bit=100
    flip_bit=7
    log_file="0317_{0}_{1}_int8_clean_abort300-500K_{2}.txt".format(total_bit, flip_bit, t)
    with open(log_file, 'w') as log:
        for i in range(times+1):
            command = f'sudo ./resnet50_inference_error -d resnet50.engine ../images_cfg.txt  ../nwpu_labels.txt {total_bit} {flip_bit} 0 {i} {t}'
            print(f'Running: {command}')
            start_time = time.time()

            result = subprocess.run(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)

            end_time = time.time()
            elapsed_time = end_time - start_time
            total_accumulated_time += elapsed_time

            # Write the output and error to log file
            print(f'Run {i+1}: Elapsed time {elapsed_time} seconds\n')
            log.write(f'Run {i+1}: Elapsed time {elapsed_time} seconds\n')
            log.write(result.stdout)
            log.write(result.stderr)
            log.flush()

            # Extract accuracy from the output
            accuracy_match = re.search(r'Accuracy: ([0-9.]+)', result.stdout)
            if accuracy_match:
                accuracy = float(accuracy_match.group(1))
                accuracies.append(accuracy)
                print(f'---------------------------------------\n')
                print(accuracy)
            else:
                print("No result\n-----\n")

        print(f'Successed : {len(accuracies)}')
        log.write(f'Successed : {len(accuracies)} in {times+1}\n')
        print(f'1000 tests 500 imgs\n Total accumulated time: {total_accumulated_time} seconds\n')
        log.write(f'1000 tests 500 imgs\nTotal accumulated time: {total_accumulated_time} seconds\n')
        log.flush()
        if accuracies:
            avg_accuracy = sum(accuracies) / len(accuracies)
            min_accuracy = min(accuracies)
            max_accuracy = max(accuracies)
            print(f'Average Accuracy: {avg_accuracy}')
            print(f'Minimum Accuracy: {min_accuracy}')
            print(f'Maximum Accuracy: {max_accuracy}')
            
            log.write(f'Average Accuracy: {avg_accuracy}\n')
            log.write(f'Minimum Accuracy: {min_accuracy}\n')
            log.write(f'Maximum Accuracy: {max_accuracy}\n')
            log.flush()
if __name__ == '__main__':
    run_resnet50(1000, 2)
