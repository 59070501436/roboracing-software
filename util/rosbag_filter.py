import rosbag
import rospy
import argparse


def main():
    parser = argparse.ArgumentParser(description='Filter rosbag files.')
    parser.add_argument("--in-bag", type=str, required=True, help="Bag file this is in")
    parser.add_argument("--start-time", type=long, required=False, default=0, help="Excludes messages before this time")
    parser.add_argument("--end-time", type=long, required=False, default=None, help="Excludes messages after this time")
    parser.add_argument("--exclude-estop", type=bool, required=False, default=False, help="Excludes messages if in estop mode")
    parser.add_argument("--only-moving", type=bool, required=False, default=False, help="Excludes messages if chassis is not moving")
    parser.add_argument("--compress", type=bool, required=False, default=False, help="Creates a compressed archive of a bag file")

    args = parser.parse_args()

    file_path = args.in_bag
    start_time = args.start_time
    end_time = args.end_time
    compression = args.compress
    only_moving = args.only_moving
    exclude_estop = args.exclude_estop
    use_index = []

    bag = rosbag.Bag(args.in_bag, 'r')
    filtered_bag = rosbag.Bag(file_path[0:file_path.rfind('.bag')] + '_filter.bag', 'w')

    if end_time is None:
        end_time = bag.get_end_time() - bag.get_start_time()

    print(bag)

    for topic, message, t in bag.read_messages(topics=['/chassis_state']):
        print not (message.estop_on == True and exclude_estop)

        use_index.append(
            start_time + bag.get_start_time() <= message.header.stamp.secs <= end_time + bag.get_start_time()
            and not (message.estop_on == True and exclude_estop)
            and not (message.speed_mps == 0 and only_moving))

    count = 0

    for write_topic, write_message, write_t in bag.read_messages():
        if use_index[count]:
            filtered_bag.write(topic=write_topic, msg=write_message, t=write_t)
        if write_topic == "/tf":
            count += 1

    bag.close()
    filtered_bag.close()


if __name__ == "__main__":
    main()
