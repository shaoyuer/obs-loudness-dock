'''
This example script shows how to interact with loudness-dock plugin through obs-websocket.
'''

import argparse
import obsws_python


def _get_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('-r', '--reset', action='store_true')
    parser.add_argument('-p', '--pause', action='store_true')
    parser.add_argument('-s', '--resume', action='store_true')
    parser.add_argument('-l', '--list-names', action='store_true')
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('--name', action='store', default=None)
    return parser.parse_args()

def _main():
    args = _get_args()

    cl = obsws_python.ReqClient(host='localhost', port=4455)

    data = {}
    if args.name:
        data['name'] = args.name

    if args.list_names:
        res = cl.send('CallVendorRequest', {
            'vendorName': 'loudness-dock',
            'requestType': 'get_names',
        })
        print(res.response_data)
        return

    if args.pause:
        cl.send('CallVendorRequest', {
            'vendorName': 'loudness-dock',
            'requestType': 'pause',
            'requestData': data | {'pause': True},
        })

    if args.reset:
        cl.send('CallVendorRequest', {
            'vendorName': 'loudness-dock',
            'requestType': 'reset',
            'requestData': data,
        })

    if args.resume:
        cl.send('CallVendorRequest', {
            'vendorName': 'loudness-dock',
            'requestType': 'pause',
            'requestData': data | {'pause': False},
        })

    if not (args.pause or args.resume or args.reset):
        if args.verbose:
            data |= {'verbose': True}
        res = cl.send('CallVendorRequest', {
            'vendorName': 'loudness-dock',
            'requestType': 'get_loudness',
            'requestData': data,
        })
        for field in ('momentary', 'short', 'integrated', 'range', 'peak'):
            value = res.response_data[field]
            if value is None:
                value = float('-inf')
            print(f'{field}: {value:.1f}')
        if args.verbose:
            print(f'duration: {res.response_data["duration"]:.1f}')
            print(f'paused: {res.response_data["paused"]}')


if __name__ == '__main__':
    _main()
